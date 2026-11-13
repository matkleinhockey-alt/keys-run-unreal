#pragma once

#include "CoreMinimal.h"

// Spoken lines through the system's own speech synthesizer (web audio/music/dj.ts speak(), which uses the browser's
// speechSynthesis): the most natural English voice on the machine, preferring a US male one. Rate and pitch are the
// web's (1 = normal). Mac only; elsewhere it's silent and the line stays on screen as text.
namespace KeysSpeech
{
	void Speak(const FString& Text, float Rate = 1.f, float Pitch = 1.f, float Volume = 1.f, float DelaySecs = 0.f);
	void StopAll();
	FString VoiceName();
}
