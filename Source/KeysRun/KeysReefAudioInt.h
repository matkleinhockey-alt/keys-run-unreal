#pragma once

#include "KeysReefAudio.h"

// Shared by the reef audio's own files only.
namespace KeysReefAudio
{
	// one sound the lane can make: its name (the test command's and the log's), the distance at which its notes are
	// at the gains written in its recipe, the distance past which it is never played, and whether it belongs under
	// water (a reef sound) or above it (a mangrove sound)
	struct FSoundInfo { const TCHAR* Name; float RefM, MaxM; bool bReef; };
	const TArray<FSoundInfo>& Sounds();
	const FSoundInfo* FindSound(const FString& Name);
	// the recipe: fills Out with the notes at the reference distance (L = R = 1) and says in Desc exactly what they are
	bool MakeSound(const FString& Name, FRandomStream& Rng, TArray<FKeysReefNote>& Out, FString& Desc);

	// what the bed is told, and the function of place and hour that makes it (the whole of criterion 1)
	struct FBedIn
	{
		double Cover = 0;        // 0..1: reef or hard bottom under the listener or within about 30 m (FState::Cover)
		double WaterDepth = 5;   // the water's depth here, m
		double DiverDepth = 2;   // how far under the surface the listener is, m
		double Hour = 12;        // the feeding clock
		double Sea = 1;          // the sea state factor (KeysGeo::SeaSW / SeaCH)
		double Swell = .5;       // 0..1: where the surge is in its cycle
	};
	FKeysReefBed BedFor(const FBedIn& In);
	// 0..1 round dawn (about 6:10) and dusk (about 19:10)
	double DuskAt(double Hour);
}
