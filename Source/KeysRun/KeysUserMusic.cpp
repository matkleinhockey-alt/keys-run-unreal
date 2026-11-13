#include "KeysUserMusic.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#import <AVFoundation/AVFoundation.h>
#endif

namespace
{
	TArray<FString> GFiles;
	int32 GCur = -1;
	bool GActive = false;
#if PLATFORM_MAC
	AVAudioPlayer* GPlayer = nil;
#endif
}

namespace KeysUserMusic
{
	FString Folder()
	{
		return FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Music"), TEXT("Keys Run"));
	}

	int32 Scan()
	{
		const FString Dir = Folder();
		IFileManager& FM = IFileManager::Get();
		if (!FM.DirectoryExists(*Dir)) FM.MakeDirectory(*Dir, true);
		TArray<FString> Found;
		FM.FindFiles(Found, *Dir, nullptr);
		GFiles.Reset();
		for (const FString& F : Found)
		{
			const FString Ext = FPaths::GetExtension(F).ToLower();
			if (Ext == TEXT("mp3") || Ext == TEXT("m4a") || Ext == TEXT("aac") || Ext == TEXT("wav") || Ext == TEXT("aif") || Ext == TEXT("aiff") || Ext == TEXT("flac") || Ext == TEXT("caf"))
				GFiles.Add(FPaths::Combine(Dir, F));
		}
		GFiles.Sort();
		return GFiles.Num();
	}

	int32 Count() { return GFiles.Num(); }
	int32 Current() { return GCur; }
	bool IsActive() { return GActive; }

	FString Title()
	{
		if (!GFiles.IsValidIndex(GCur)) return FString();
		return FPaths::GetBaseFilename(GFiles[GCur]).Left(40);
	}

	void Stop()
	{
		GActive = false;
#if PLATFORM_MAC
		if (GPlayer) { [GPlayer stop]; [GPlayer release]; GPlayer = nil; }
#endif
	}

	bool Play(int32 Index)
	{
		if (!GFiles.Num() && !Scan()) return false;
#if PLATFORM_MAC
		// a file that won't decode is skipped rather than ending the station
		for (int32 Try = 0; Try < GFiles.Num(); ++Try)
		{
			const int32 I = ((Index + Try) % GFiles.Num() + GFiles.Num()) % GFiles.Num();
			if (GPlayer) { [GPlayer stop]; [GPlayer release]; GPlayer = nil; }
			@autoreleasepool
			{
				NSURL* Url = [NSURL fileURLWithPath:GFiles[I].GetNSString()];
				NSError* Err = nil;
				AVAudioPlayer* P = [[AVAudioPlayer alloc] initWithContentsOfURL:Url error:&Err];
				if (!P || Err) { if (P) [P release]; continue; }
				P.volume = 0;
				[P prepareToPlay];
				if (![P play]) { [P release]; continue; }
				GPlayer = P;
			}
			GCur = I;
			GActive = true;
			UE_LOG(LogTemp, Log, TEXT("KRMusic playing %s"), *FPaths::GetCleanFilename(GFiles[I]));
			return true;
		}
#endif
		GActive = false;
		return false;
	}

	void Update(float Volume, float Pan)
	{
		if (!GActive) return;
#if PLATFORM_MAC
		if (!GPlayer) return;
		GPlayer.volume = FMath::Clamp(Volume, 0.f, 1.f);
		GPlayer.pan = FMath::Clamp(Pan, -1.f, 1.f);
		if (!GPlayer.playing) Play(GCur + 1);   // on to the next song
#endif
	}
}
