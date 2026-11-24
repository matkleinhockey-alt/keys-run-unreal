#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// The deep bait balls and what works them (the boids are KeysSchools'; this is what the rest of the game can ask about
// them, and the bodies of the offshore bait, the silky shark and the birds).
//
// Real: off the reef line and out in the Gulf Stream bait balls up in the top of the water: ballyhoo, Spanish sardine,
// round scad (cigar minnow), with flying fish about. Blackfin and skipjack tuna crash them from below in packs, mahi come
// in small groups, a sailfish or two herd them with the dorsal raised and slash through, king mackerel and wahoo make
// fast runs through them, a silky shark or a pod of bottlenose dolphins joins in now and then. Pushed to the surface
// the ball showers (bait jumping clear, flying fish gliding off) and the birds come: terns plunging in, frigatebirds
// wheeling high above. Over the reef there are fewer and smaller; none inside Hawk Channel.
namespace KeysBait
{
	// one bait ball, as the camera's surroundings have it now (game metres, y up)
	struct FBall
	{
		int64 Site = 0;
		double X = 0, Y = -8, Z = 0;
		double R = 3;            // its radius
		double Work = 0;         // 0 quiet in mid-water .. 1 crashed at the surface
		bool bLive = false;      // its fish are in the water now (near the camera); else dealt but not yet put
		bool bDeep = true;       // off soundings (over 30 m); false: a small one over the reef
		FString Bait;            // ballyhoo, sardine, scad
		TArray<TPair<FString, int32>> Preds;   // KeysSchools keys of what works it, and how many
	};

	// KeysSchools, twice a second: every ball within reach of the camera
	void Publish(TArray<FBall>&& Balls);
	const TArray<FBall>& Balls();
	// KeysSchools, four times a second: the ball fish and their predators near the boat (x, y, z, length)
	void PublishSonar(TArray<FVector4>&& Fish);

	// ---- hub hooks (one call each)
	// KeysLife::PickSpecies: a cast or a troll at a worked ball bites with the fish working it, most of the time;
	// empty when the ball has no say. Rand in [0, 1).
	FString PickAt(double X, double Z, double Rand);
	// KeysLife::SpotAt: a worked ball fishes as a hot spot (bFrenzy: crashed at the surface)
	bool WorkedAt(double X, double Z, bool* bFrenzy = nullptr);
	// KeysHUD's sounder: the ball fish and the predators within R of (X, Z)
	void SonarNear(double X, double Z, double R, TArray<FVector>& OutPos, TArray<double>& OutLen);

	// the bodies KeysSchools draws that KeysLife has no model of: ballyhoo, sardine, scad, silky (repainted from
	// barracuda, herring, blue runner and blacktip), tern and frigatebird (built here). Nose along +X, as
	// AKeysLife::BuildSchoolMesh. False for any other key.
	bool BuildMesh(const FString& Key, FKeysMesh& Out, int32 Lod);
	// the KeysData key a predator is caught as, or empty (dolphins and the silky are not caught)
	FString CatchKey(const FString& SchoolKey);
}
