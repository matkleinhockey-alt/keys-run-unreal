#pragma once

#include "CoreMinimal.h"

class UWorld;

// Settings > Graphics: the quality preset (Low / Medium / High / Epic), the render resolution (Auto follows the preset)
// and the window. Kept in GameUserSettings.ini under [KeysRun.Graphics]; test runs (-KRScript) never write it.
//
// On a Retina Mac the game draws at the panel's native pixels (Info.plist NSHighResolutionCapable) and TSR upscales
// from a fraction of them: Low 50%, Medium 62%, High 75%, Epic 100%. The first launch picks High on a Pro / Max /
// Ultra chip (or any discrete GPU) and Medium on a base Apple chip (the MacBook Air), full screen at the desktop size.
namespace KeysGfx
{
	enum EQuality : int32 { Low = 0, Medium = 1, High = 2, Epic = 3 };

	// once, when the game starts
	void Init(UWorld* World);
	// a periodic "KRGFX" log line with the frame time and the render size (for offscreen comparisons)
	void Tick(UWorld* World, double Dt);

	int32 Quality();
	void SetQuality(int32 Q);
	// 0 = Auto (the preset's), else 50..100 percent of the window's pixels
	int32 ResScale();
	void SetResScale(int32 Pct);
	int32 EffectiveScreenPercent();
	bool IsFullscreen();
	void SetFullscreen(bool bOn);

	FString QualityLabel();
	FString ResScaleLabel();
	FString Describe();   // what the current preset does, for the settings help panel

	// the preset's cheaper look (the world reads these): volumetric clouds, contact shadows, the reef's density
	bool CloudsOn();
	bool ContactShadowsOn();
}
