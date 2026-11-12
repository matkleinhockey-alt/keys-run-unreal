#pragma once

#include "CoreMinimal.h"

// Small real-time DSP kit used by the Keys Run synths. It mirrors the WebAudio nodes the JS game builds its sound from:
// oscillators (sine / square / sawtooth / triangle / a periodic wave), the looping 2 s noise buffer, BiquadFilterNode
// (lowpass / highpass / bandpass, with WebAudio's Q conventions), WaveShaper, setTargetAtTime smoothing, and a
// DynamicsCompressor stand-in. Nothing here allocates on the audio thread.
namespace KeysDSP
{
	enum class EWave : uint8 { Sine, Square, Saw, Triangle, Engine };
	enum class EFilter : uint8 { Lowpass, Highpass, Bandpass };

	constexpr float TwoPi = 6.28318530718f;

	// shared tables, built once (thread-safe function statics)
	const TArray<float>& NoiseTable();   // the JS AUD.noise buffer: white plus a leaky-integrated rumble, 2 s at 48 kHz
	const TArray<float>& SineTable();    // 4096 points, one cycle
	const TArray<float>& EngineTable();  // engineWave(v8, rough=.8): the cross-plane V8 periodic wave, peak-normalized

	float SineLookup(float Phase01);
	float TableLookup(const TArray<float>& T, float Phase01);

	inline float MidiToHz(float M) { return 440.f * FMath::Pow(2.f, (M - 69.f) / 12.f); }
	inline float CentsToMul(float C) { return FMath::Pow(2.f, C / 1200.f); }
	// setTargetAtTime: the fraction of the remaining distance covered in Dt seconds with time constant Tau
	inline float SmoothK(float Dt, float Tau) { return Tau <= 0.f ? 1.f : 1.f - FMath::Exp(-Dt / Tau); }
	// exponentialRampToValueAtTime from A to B over the ramp, at fraction U (clamped)
	inline float ExpRamp(float A, float B, float U)
	{
		U = FMath::Clamp(U, 0.f, 1.f);
		if (A <= 0.f || B <= 0.f) return A + (B - A) * U;
		return A * FMath::Pow(B / A, U);
	}

	struct FOsc
	{
		float Phase = 0.f;
		EWave Wave = EWave::Sine;
		// Inc = frequency / sample rate
		float Tick(float Inc);
	};

	struct FNoise
	{
		int32 Pos = 0;
		void Seed(FRandomStream& R);
		float Tick();
	};

	struct FBiquad
	{
		EFilter Type = EFilter::Lowpass;
		float B0 = 1, B1 = 0, B2 = 0, A1 = 0, A2 = 0;
		float Z1 = 0, Z2 = 0;
		float LastF = -1, LastQ = -1;
		// Q follows WebAudio: in dB for lowpass / highpass, plain Q for bandpass
		void Set(EFilter InType, float Freq, float Q, float SampleRate);
		float Tick(float X);
		void Reset() { Z1 = Z2 = 0; }
	};

	// WaveShaper with the JS audShaper(k) curve
	inline float Shape(float X, float K)
	{
		X = FMath::Clamp(X, -1.f, 1.f);
		return (1.f + K) * X / (1.f + K * FMath::Abs(X));
	}

	struct FSmooth
	{
		float V = 0, Target = 0, Tau = .05f;
		void Init(float Val, float InTau) { V = Target = Val; Tau = InTau; }
		void Step(float Dt) { V += (Target - V) * SmoothK(Dt, Tau); }
	};

	// DynamicsCompressor stand-in (threshold -14 dB, ratio 4, soft knee, attack 3 ms, release 250 ms, WebAudio-like makeup)
	struct FCompressor
	{
		float Env = 0;
		float Gain = 1;
		void Process(float* Interleaved, int32 Frames, int32 Channels, float SampleRate);
	};

	// equal-power stereo placement plus the WebAudio "inverse" distance model, from game-thread positions
	void Spatialize(const FVector& ListenerUE, const FVector& ListenerRightUE, const FVector& SourceUE, float RefDistM, float Rolloff, float& OutL, float& OutR);
}
