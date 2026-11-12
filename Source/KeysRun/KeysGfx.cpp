#include "KeysGfx.h"
#include "Scalability.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "RHI.h"
#include "UnrealClient.h"

namespace
{
	const TCHAR* Section = TEXT("KeysRun.Graphics");
	int32 GQuality = KeysGfx::Medium;
	int32 GResScale = 0;
	int32 GAppliedPct = -1;
	FIntPoint GAppliedSize(0, 0);
	bool GInit = false;
	bool GNoSave = false;
	double GLogT = 0, GAccMs = 0;
	int32 GFrames = 0;

	// render height Auto aims for per preset: TSR fills in the rest of a Retina panel's pixels
	const double TargetH[4] = {800, 1000, 1250, 1e9};

	bool Scripted() { return FString(FCommandLine::Get()).Contains(TEXT("KRScript=")); }

	FIntPoint ViewportSize()
	{
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) return GEngine->GameViewport->Viewport->GetSizeXY();
		return FIntPoint(0, 0);
	}

	void SetCVar(const TCHAR* Name, float V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(V, ECVF_SetByGameSetting);
	}
	void SetCVarInt(const TCHAR* Name, int32 V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(V, ECVF_SetByGameSetting);
	}

	int32 DefaultQuality()
	{
		// Apple: the base chips (MacBook Air, the 13" Pro, the mini) get Medium; Pro / Max / Ultra get High
		const FString Gpu = GRHIAdapterName.IsEmpty() ? FPlatformMisc::GetPrimaryGPUBrand() : GRHIAdapterName;
		if (Gpu.Contains(TEXT("Apple")))
			return (Gpu.Contains(TEXT("Pro")) || Gpu.Contains(TEXT("Max")) || Gpu.Contains(TEXT("Ultra"))) ? KeysGfx::High : KeysGfx::Medium;
		return KeysGfx::High;
	}

	void Save()
	{
		if (GNoSave || !GConfig) return;
		GConfig->SetInt(Section, TEXT("Quality"), GQuality, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("ResScale"), GResScale, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	int32 PercentFor(const FIntPoint& Size)
	{
		if (GResScale > 0) return FMath::Clamp(GResScale, 50, 100);
		if (Size.Y <= 0 || GQuality >= KeysGfx::Epic) return 100;
		return FMath::Clamp(FMath::RoundToInt(100 * TargetH[GQuality] / Size.Y), 50, 100);
	}

	void ApplyResolution()
	{
		const FIntPoint Size = ViewportSize();
		const int32 Pct = PercentFor(Size);
		GAppliedSize = Size;
		if (Pct == GAppliedPct) return;
		GAppliedPct = Pct;
		SetCVar(TEXT("r.ScreenPercentage"), float(Pct));
		UE_LOG(LogTemp, Log, TEXT("KRGFX resolution: window %dx%d, screen percentage %d -> renders %dx%d, TSR upscales"),
			Size.X, Size.Y, Pct, Size.X * Pct / 100, Size.Y * Pct / 100);
	}

	void ApplyQuality()
	{
		const int32 Q = GQuality;
		Scalability::FQualityLevels L = Scalability::GetQualityLevels();
		const int32 G = Q == KeysGfx::Low ? 1 : Q == KeysGfx::Medium ? 2 : 3;
		L.SetFromSingleQualityLevel(G);
		L.ViewDistanceQuality = 3;                                     // the Keys are all horizon: never pull it in
		L.AntiAliasingQuality = Q == KeysGfx::Epic ? 3 : 2;            // Epic keeps TSR's 200% history
		L.TextureQuality = 3;                                          // there are no textures to speak of
		Scalability::SetQualityLevels(L, true);
		GAppliedPct = -1;
		ApplyResolution();

		// sharpen what TSR upscales, more when it upscales more
		SetCVar(TEXT("r.Tonemapper.Sharpen"), Q == KeysGfx::Low ? 1.f : Q == KeysGfx::Medium ? .8f : Q == KeysGfx::High ? .6f : .4f);
		// Lumen: the final gather and reflections a notch finer on the bigger presets
		SetCVar(TEXT("r.Lumen.ScreenProbeGather.ScreenSpaceBentNormal"), Q >= KeysGfx::Medium ? 1.f : 0.f);
		SetCVarInt(TEXT("r.Lumen.Reflections.DownsampleFactor"), Q >= KeysGfx::High ? 1 : 2);
		SetCVarInt(TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"), Q >= KeysGfx::Epic ? 16 : Q >= KeysGfx::High ? 16 : 32);
		// contact shadows and screen space AO
		SetCVarInt(TEXT("r.ContactShadows"), Q >= KeysGfx::Medium ? 1 : 0);
		SetCVarInt(TEXT("r.AmbientOcclusion.Method"), 0);
		// the clouds: half the ray-march steps below High
		SetCVar(TEXT("r.VolumetricCloud.ViewRaySampleMaxCount"), Q >= KeysGfx::High ? 768.f : 256.f);
		SetCVar(TEXT("r.VolumetricCloud.ReflectionRaySampleMaxCount"), Q >= KeysGfx::High ? 80.f : 24.f);
		SetCVarInt(TEXT("r.VolumetricCloud.EnableAerialPerspectiveSampling"), Q >= KeysGfx::High ? 1 : 0);
		UE_LOG(LogTemp, Log, TEXT("KRGFX quality %s (scalability %d), resolution scale %s, GPU %s"), *KeysGfx::QualityLabel(), G, *KeysGfx::ResScaleLabel(), *GRHIAdapterName);
	}
}

namespace KeysGfx
{
	void Init(UWorld* World)
	{
		if (GInit) return;
		GInit = true;
		GNoSave = Scripted() || !World || World->WorldType == EWorldType::PIE;
		bool bFirst = true;
		if (GConfig)
		{
			int32 Q = 0;
			if (GConfig->GetInt(Section, TEXT("Quality"), Q, GGameUserSettingsIni)) { GQuality = FMath::Clamp(Q, 0, 3); bFirst = false; }
			else GQuality = DefaultQuality();
			GConfig->GetInt(Section, TEXT("ResScale"), GResScale, GGameUserSettingsIni);
		}
		int32 Cmd = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("KRGfx="), Cmd)) GQuality = FMath::Clamp(Cmd, 0, 3);
		if (Scripted() && Cmd < 0) GQuality = DefaultQuality();
		FParse::Value(FCommandLine::Get(), TEXT("KRResScale="), GResScale);

		// the first launch of the real game: full screen at the display's own size (its Retina pixels), not the
		// engine's 1280 x 720 window that macOS then blows up
		if (bFirst && !GNoSave && !GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")) && !FParse::Param(FCommandLine::Get(), TEXT("windowed")))
			SetFullscreen(true);
		ApplyQuality();
		Save();
	}

	void Tick(UWorld* World, double Dt)
	{
		if (!GInit) return;
		if (ViewportSize() != GAppliedSize) ApplyResolution();
		GAccMs += Dt * 1000;
		GFrames++;
		GLogT += Dt;
		if (GLogT >= 5 && GFrames > 0)
		{
			const double GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
			const FIntPoint S = ViewportSize();
			UE_LOG(LogTemp, Log, TEXT("KRGFX frame %.1f ms (gpu %.1f ms), %s, window %dx%d at %d%%"), GAccMs / GFrames, GpuMs, *QualityLabel(), S.X, S.Y, GAppliedPct);
			GLogT = 0; GAccMs = 0; GFrames = 0;
		}
	}

	int32 Quality() { return GQuality; }
	void SetQuality(int32 Q) { GQuality = (Q + 4) % 4; ApplyQuality(); Save(); }
	int32 ResScale() { return GResScale; }
	void SetResScale(int32 Pct) { GResScale = Pct <= 0 ? 0 : FMath::Clamp(Pct, 50, 100); ApplyResolution(); Save(); }
	int32 EffectiveScreenPercent() { return GAppliedPct; }

	bool IsFullscreen()
	{
		const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		return S && S->GetFullscreenMode() != EWindowMode::Windowed;
	}

	void SetFullscreen(bool bOn)
	{
		UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!S) return;
		if (bOn)
		{
			S->SetFullscreenMode(EWindowMode::WindowedFullscreen);
			S->SetScreenResolution(S->GetDesktopResolution());
		}
		else
		{
			const FIntPoint D = S->GetDesktopResolution();
			S->SetFullscreenMode(EWindowMode::Windowed);
			S->SetScreenResolution(FIntPoint(D.X * 3 / 4, D.Y * 3 / 4));
		}
		S->ApplyResolutionSettings(false);
		if (!GNoSave) S->SaveSettings();
	}

	FString QualityLabel()
	{
		static const TCHAR* N[4] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic")};
		return N[FMath::Clamp(GQuality, 0, 3)];
	}

	FString ResScaleLabel()
	{
		const int32 P = GAppliedPct > 0 ? GAppliedPct : PercentFor(ViewportSize());
		return GResScale > 0 ? FString::Printf(TEXT("%d%%"), GResScale) : FString::Printf(TEXT("Auto (%d%%)"), P);
	}

	FString Describe()
	{
		switch (GQuality)
		{
		case Low: return TEXT("Low: for a MacBook Air on battery. Simpler global illumination and shadows, screen-space reflections, light clouds, and a lower render resolution that TSR sharpens up to the screen's.");
		case Medium: return TEXT("Medium: the MacBook Air's default. Lumen lighting and reflections at medium quality, contact shadows, and about 1000 lines rendered and upscaled to the full Retina resolution.");
		case High: return TEXT("High: Lumen lighting and reflections, soft virtual shadows, full volumetric clouds and about 1250 lines rendered. The default on Pro, Max and Ultra chips.");
		default: return TEXT("Epic: everything at full quality at the screen's native resolution. For a desktop Mac or an external display.");
		}
	}

	bool CloudsOn() { return true; }
	bool ContactShadowsOn() { return GQuality >= Medium; }
}
