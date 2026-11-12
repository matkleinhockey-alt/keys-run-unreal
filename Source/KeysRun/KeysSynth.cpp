#include "KeysSynth.h"

namespace KeysDSP
{
	const TArray<float>& NoiseTable()
	{
		static const TArray<float> Table = []()
		{
			// const len=sampleRate*2; for(...){ const w=Math.random()*2-1; b=.97*b+.03*w; d[i]=w*.6+b*2.5; }
			TArray<float> T;
			const int32 Len = 48000 * 2;
			T.SetNumUninitialized(Len);
			FRandomStream R(0x6b657973);
			float B = 0.f;
			for (int32 I = 0; I < Len; ++I)
			{
				const float W = R.FRand() * 2.f - 1.f;
				B = .97f * B + .03f * W;
				T[I] = W * .6f + B * 2.5f;
			}
			return T;
		}();
		return Table;
	}

	const TArray<float>& SineTable()
	{
		static const TArray<float> Table = []()
		{
			TArray<float> T;
			const int32 N = 4096;
			T.SetNumUninitialized(N + 1);
			for (int32 I = 0; I <= N; ++I) T[I] = FMath::Sin(TwoPi * (float)I / (float)N);
			return T;
		}();
		return Table;
	}

	const TArray<float>& EngineTable()
	{
		static const TArray<float> Table = []()
		{
			// engineWave(v8, rough): harmonic weights, im = H, re = H * .45 * sin(i * 1.7); WebAudio normalizes the peak to 1
			const float Rough = .8f;
			struct FH { int32 K; float A; };
			const FH H[] = {{1, .32f}, {2, .75f * (.6f + Rough)}, {3, .16f}, {4, 1.f}, {5, .08f}, {6, .32f}, {7, .05f}, {8, .46f},
				{10, .14f}, {12, .26f}, {14, .07f}, {16, .18f}, {20, .09f}, {24, .07f}, {32, .04f}};
			const int32 N = 2048;
			TArray<float> T;
			T.SetNumZeroed(N + 1);
			float Peak = 0.f;
			for (int32 I = 0; I < N; ++I)
			{
				const float Ph = TwoPi * (float)I / (float)N;
				float S = 0.f;
				for (const FH& E : H)
				{
					const float Re = E.A * .45f * FMath::Sin((float)E.K * 1.7f);
					S += Re * FMath::Cos(E.K * Ph) + E.A * FMath::Sin(E.K * Ph);
				}
				T[I] = S;
				Peak = FMath::Max(Peak, FMath::Abs(S));
			}
			if (Peak > 0.f)
				for (int32 I = 0; I < N; ++I) T[I] /= Peak;
			T[N] = T[0];
			return T;
		}();
		return Table;
	}

	float TableLookup(const TArray<float>& T, float Phase01)
	{
		const int32 N = T.Num() - 1;
		const float X = Phase01 * (float)N;
		int32 I = (int32)X;
		I = FMath::Clamp(I, 0, N - 1);
		const float F = X - (float)I;
		return T[I] + (T[I + 1] - T[I]) * F;
	}

	float SineLookup(float Phase01) { return TableLookup(SineTable(), Phase01); }

	static FORCEINLINE float PolyBlep(float T, float Dt)
	{
		if (Dt <= 0.f) return 0.f;
		if (T < Dt) { T /= Dt; return T + T - T * T - 1.f; }
		if (T > 1.f - Dt) { T = (T - 1.f) / Dt; return T * T + T + T + 1.f; }
		return 0.f;
	}

	float FOsc::Tick(float Inc)
	{
		Inc = FMath::Clamp(Inc, 0.f, .49f);
		float Out = 0.f;
		switch (Wave)
		{
		case EWave::Sine: Out = SineLookup(Phase); break;
		case EWave::Engine: Out = TableLookup(EngineTable(), Phase); break;
		case EWave::Saw: Out = 2.f * Phase - 1.f - PolyBlep(Phase, Inc); break;
		case EWave::Square:
		{
			Out = Phase < .5f ? 1.f : -1.f;
			Out += PolyBlep(Phase, Inc);
			float P2 = Phase + .5f;
			if (P2 >= 1.f) P2 -= 1.f;
			Out -= PolyBlep(P2, Inc);
			break;
		}
		case EWave::Triangle: Out = Phase < .5f ? 4.f * Phase - 1.f : 3.f - 4.f * Phase; break;
		}
		Phase += Inc;
		if (Phase >= 1.f) Phase -= 1.f;
		return Out;
	}

	void FNoise::Seed(FRandomStream& R)
	{
		// s.start(0, Math.random()*1.5..1.8): every source reads the shared buffer from its own offset
		Pos = R.RandRange(0, FMath::Max(0, NoiseTable().Num() - 1));
	}

	float FNoise::Tick()
	{
		const TArray<float>& T = NoiseTable();
		const float V = T[Pos];
		if (++Pos >= T.Num()) Pos = 0;
		return V;
	}

	void FBiquad::Set(EFilter InType, float Freq, float Q, float SampleRate)
	{
		Freq = FMath::Clamp(Freq, 10.f, SampleRate * .45f);
		if (InType == Type && FMath::IsNearlyEqual(Freq, LastF, .01f) && Q == LastQ) return;
		Type = InType;
		LastF = Freq;
		LastQ = Q;
		const float W0 = TwoPi * Freq / SampleRate;
		const float Cs = FMath::Cos(W0), Sn = FMath::Sin(W0);
		float Alpha, A0;
		switch (Type)
		{
		case EFilter::Lowpass:
			Alpha = Sn / (2.f * FMath::Pow(10.f, Q / 20.f));
			B0 = (1.f - Cs) * .5f; B1 = 1.f - Cs; B2 = (1.f - Cs) * .5f;
			break;
		case EFilter::Highpass:
			Alpha = Sn / (2.f * FMath::Pow(10.f, Q / 20.f));
			B0 = (1.f + Cs) * .5f; B1 = -(1.f + Cs); B2 = (1.f + Cs) * .5f;
			break;
		default:
			Alpha = Sn / (2.f * FMath::Max(Q, .0001f));
			B0 = Alpha; B1 = 0.f; B2 = -Alpha;
			break;
		}
		A0 = 1.f + Alpha;
		A1 = -2.f * Cs;
		A2 = 1.f - Alpha;
		B0 /= A0; B1 /= A0; B2 /= A0; A1 /= A0; A2 /= A0;
	}

	float FBiquad::Tick(float X)
	{
		// transposed direct form II
		const float Y = B0 * X + Z1;
		Z1 = B1 * X - A1 * Y + Z2;
		Z2 = B2 * X - A2 * Y;
		return Y;
	}

	static float CompCurveDb(float X)
	{
		const float Thr = -14.f, Knee = 30.f, Ratio = 4.f;
		if (X <= Thr) return X;
		if (X <= Thr + Knee)
		{
			const float D = X - Thr;
			return X + (1.f / Ratio - 1.f) * D * D / (2.f * Knee);
		}
		const float AtKnee = Thr + Knee + (1.f / Ratio - 1.f) * Knee * .5f;
		return AtKnee + (X - Thr - Knee) / Ratio;
	}

	void FCompressor::Process(float* Buf, int32 Frames, int32 Channels, float SampleRate)
	{
		static const float Makeup = FMath::Pow(10.f, -.6f * CompCurveDb(0.f) / 20.f);
		const float AttK = SmoothK(1.f / SampleRate, .003f);
		const float RelK = SmoothK(1.f / SampleRate, .25f);
		for (int32 F = 0; F < Frames; ++F)
		{
			float Peak = 0.f;
			for (int32 C = 0; C < Channels; ++C) Peak = FMath::Max(Peak, FMath::Abs(Buf[F * Channels + C]));
			Env += (Peak - Env) * (Peak > Env ? AttK : RelK);
			const float Db = 8.68588964f * FMath::Loge(FMath::Max(Env, 1e-6f));
			Gain = FMath::Pow(10.f, (CompCurveDb(Db) - Db) / 20.f) * Makeup;
			for (int32 C = 0; C < Channels; ++C)
			{
				float& S = Buf[F * Channels + C];
				// last-resort clamp so nothing wraps
				S = FMath::Clamp(S * Gain, -1.f, 1.f);
			}
		}
	}

	void Spatialize(const FVector& L, const FVector& Right, const FVector& Src, float Ref, float Rolloff, float& OutL, float& OutR)
	{
		const FVector D = Src - L;
		const float DistM = (float)D.Size() / 100.f;
		const float Gain = Ref / (Ref + Rolloff * (FMath::Max(DistM, Ref) - Ref));
		float Pan = 0.f;
		if (DistM > .01f) Pan = (float)FVector::DotProduct(D.GetSafeNormal(), Right) * .8f;
		const float X = (Pan + 1.f) * .5f;
		OutL = Gain * FMath::Cos(X * UE_PI * .5f) * 1.41421f;
		OutR = Gain * FMath::Sin(X * UE_PI * .5f) * 1.41421f;
	}
}
