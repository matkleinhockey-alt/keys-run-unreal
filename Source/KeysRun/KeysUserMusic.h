#pragma once

#include "CoreMinimal.h"

// "My Music": your own songs on the boat radio (web audio/music/radio.ts loadFile). Drop audio files (mp3, m4a, aac,
// wav, aiff, flac, caf) into ~/Music/Keys Run and P cycles to the station. macOS decodes and plays them; the game sets
// the volume and pan each frame from where the boat's speakers are, so it still sounds like it's on the boat.
namespace KeysUserMusic
{
	FString Folder();
	int32 Scan();                 // (re)lists the folder, creating it if missing; returns how many songs
	int32 Count();
	bool Play(int32 Index);       // starts that song (wraps); false if there are none or it won't play
	void Stop();
	bool IsActive();
	void Update(float Volume, float Pan);   // per frame while the station is on: placement, and on to the next song at the end
	int32 Current();
	FString Title();
}
