#include "KeysSpeech.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"

#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#import <AVFoundation/AVFoundation.h>

namespace
{
	AVSpeechSynthesizer* GSynth = nil;
	AVSpeechSynthesisVoice* GVoice = nil;
	bool GPicked = false;

	// web bestVoice(): neural / enhanced first, a few known good names, en-US, male; never the compact novelty ones
	int32 Score(AVSpeechSynthesisVoice* V)
	{
		const FString Name(V.name), Lang(V.language);
		int32 S = 0;
		if (V.quality == AVSpeechSynthesisVoiceQualityPremium) S += 12;
		else if (V.quality == AVSpeechSynthesisVoiceQualityEnhanced) S += 10;
		for (const TCHAR* N : {TEXT("Evan"), TEXT("Nathan"), TEXT("Aaron"), TEXT("Tom"), TEXT("Alex"), TEXT("Reed"), TEXT("Daniel"), TEXT("Jamie")})
			if (Name.Contains(N)) { S += 4; break; }
		if (Lang == TEXT("en-US")) S += 3;
		if (V.gender == AVSpeechSynthesisVoiceGenderMale) S += 2;
		if (V.gender == AVSpeechSynthesisVoiceGenderFemale) S -= 3;
		for (const TCHAR* N : {TEXT("Bad News"), TEXT("Bahh"), TEXT("Bells"), TEXT("Boing"), TEXT("Bubbles"), TEXT("Cellos"), TEXT("Wobble"), TEXT("Zarvox"), TEXT("Trinoids"), TEXT("Whisper"), TEXT("Jester"), TEXT("Organ"), TEXT("Superstar"), TEXT("Good News"), TEXT("Albert"), TEXT("Junior"), TEXT("Grandpa"), TEXT("Grandma"), TEXT("Ralph"), TEXT("Kathy"), TEXT("Fred")})
			if (Name.Contains(N)) { S -= (FString(N) == TEXT("Fred") || FString(N) == TEXT("Ralph") || FString(N) == TEXT("Kathy")) ? 6 : 30; break; }
		return S;
	}

	void Pick()
	{
		if (GPicked) return;
		GPicked = true;
		int32 Best = -1000;
		for (AVSpeechSynthesisVoice* V in [AVSpeechSynthesisVoice speechVoices])
		{
			if (![V.language hasPrefix:@"en"]) continue;
			const int32 S = Score(V);
			if (S > Best) { Best = S; GVoice = V; }
		}
		if (GVoice) [GVoice retain];
		UE_LOG(LogTemp, Log, TEXT("KRSpeech voice: %s"), GVoice ? *(FString(GVoice.name) + TEXT(" / ") + FString(GVoice.language) + FString::Printf(TEXT(" quality %d"), (int32)GVoice.quality)) : TEXT("none"));
		GSynth = [[AVSpeechSynthesizer alloc] init];
	}

	void SpeakNow(const FString& Text, float Rate, float Pitch, float Volume)
	{
		@autoreleasepool
		{
			Pick();
			if (!GSynth) return;
			AVSpeechUtterance* U = [AVSpeechUtterance speechUtteranceWithString:Text.GetNSString()];
			if (GVoice) U.voice = GVoice;
			// web rate 1 = normal speech; AVSpeech's normal is AVSpeechUtteranceDefaultSpeechRate
			U.rate = FMath::Clamp(AVSpeechUtteranceDefaultSpeechRate * Rate, AVSpeechUtteranceMinimumSpeechRate, AVSpeechUtteranceMaximumSpeechRate);
			U.pitchMultiplier = FMath::Clamp(Pitch, .5f, 2.f);
			U.volume = FMath::Clamp(Volume, 0.f, 1.f);
			[GSynth speakUtterance:U];
			UE_LOG(LogTemp, Log, TEXT("KRSpeech: \"%s\" rate %.2f pitch %.2f"), *Text, Rate, Pitch);
		}
	}
}
#endif

namespace KeysSpeech
{
	void Speak(const FString& Text, float Rate, float Pitch, float Volume, float DelaySecs)
	{
#if PLATFORM_MAC
		if (Text.IsEmpty() || FParse::Param(FCommandLine::Get(), TEXT("nosound"))) return;
		if (DelaySecs > 0)
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Text, Rate, Pitch, Volume](float) { SpeakNow(Text, Rate, Pitch, Volume); return false; }), DelaySecs);
			return;
		}
		SpeakNow(Text, Rate, Pitch, Volume);
#endif
	}

	void StopAll()
	{
#if PLATFORM_MAC
		if (GSynth) [GSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
#endif
	}

	FString VoiceName()
	{
#if PLATFORM_MAC
		Pick();
		return GVoice ? FString(GVoice.name) + TEXT(" (") + FString(GVoice.language) + TEXT(")") : FString(TEXT("none"));
#else
		return TEXT("none");
#endif
	}
}
