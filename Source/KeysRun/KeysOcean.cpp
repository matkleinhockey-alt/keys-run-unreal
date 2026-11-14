#include "KeysOcean.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr int32 N = 64;
	constexpr double G = 9.81;
	constexpr double Split = 8.0;   // m: waves longer than this live on the big tile, shorter on the small one

	struct FCx { double R = 0, I = 0; };
	FORCEINLINE FCx Mul(const FCx& A, const FCx& B) { return {A.R * B.R - A.I * B.I, A.R * B.I + A.I * B.R}; }
	FORCEINLINE FCx Conj(const FCx& A) { return {A.R, -A.I}; }

	// in-place radix-2 inverse DFT (no 1/N): f[n] = sum_k F[k] e^{+2 pi i k n / N}
	void Fft1(FCx* D, int32 Stride)
	{
		for (int32 i = 1, j = 0; i < N; i++)
		{
			int32 Bit = N >> 1;
			for (; j & Bit; Bit >>= 1) j ^= Bit;
			j ^= Bit;
			if (i < j) Swap(D[i * Stride], D[j * Stride]);
		}
		for (int32 Len = 2; Len <= N; Len <<= 1)
		{
			const double A = 2 * PI / Len;
			const FCx W1{FMath::Cos(A), FMath::Sin(A)};
			for (int32 i = 0; i < N; i += Len)
			{
				FCx W{1, 0};
				for (int32 k = 0; k < Len / 2; k++)
				{
					FCx& U = D[(i + k) * Stride];
					FCx& V = D[(i + k + Len / 2) * Stride];
					const FCx T = Mul(V, W);
					V = {U.R - T.R, U.I - T.I};
					U = {U.R + T.R, U.I + T.I};
					W = Mul(W, W1);
				}
			}
		}
	}
	void Fft2(TArray<FCx>& D)
	{
		for (int32 r = 0; r < N; r++) Fft1(D.GetData() + r * N, 1);
		for (int32 c = 0; c < N; c++) Fft1(D.GetData() + c, N);
	}

	struct FCascade
	{
		double L = 100;
		double KMin = 0, KMax = 1e9;           // its band
		TArray<FCx> Xi;                         // fixed complex gaussians per wave (so changing the sea never jumps)
		TArray<FCx> H0;                         // this frame's initial amplitudes
		TArray<double> Om;                      // angular frequency per wave
		TArray<float> H, Dx, Dz, Sx, Sz, J, Fm; // the fields on the grid
		double Sum2 = 0;                        // sum |H0|^2 before the rms is applied

		void Init(double InL, double InKMin, double InKMax, uint32 Seed)
		{
			L = InL; KMin = InKMin; KMax = InKMax;
			FRandomStream R{int32(Seed)};
			Xi.SetNum(N * N); H0.SetNum(N * N); Om.SetNum(N * N);
			for (TArray<float>* A : {&H, &Dx, &Dz, &Sx, &Sz, &J, &Fm}) A->Init(0.f, N * N);
			J.Init(1.f, N * N);
			for (int32 i = 0; i < N * N; i++)
			{
				// Box-Muller, E|xi|^2 = 1
				const double U1 = FMath::Max(1e-9, double(R.FRand())), U2 = R.FRand();
				const double Mg = FMath::Sqrt(-2 * FMath::Loge(U1)) / FMath::Sqrt(2.0);
				Xi[i] = {Mg * FMath::Cos(2 * PI * U2), Mg * FMath::Sin(2 * PI * U2)};
				// the Nyquist row and column have no partner wave: leave them out (they'd leak between packed fields)
				if (i % N == N / 2 || i / N == N / 2) Xi[i] = {0, 0};
			}
		}
		FORCEINLINE void K(int32 Idx, double& Kx, double& Kz) const
		{
			const int32 n = Idx % N, m = Idx / N;
			Kx = 2 * PI * (n < N / 2 ? n : n - N) / L;
			Kz = 2 * PI * (m < N / 2 ? m : m - N) / L;
		}
	};

	FCascade GC[2];
	bool GInit = false, GOn = true;
	double GT = 0, GLastT = -1;
	TArray<FCx> GBuf[4];

	void InitAll()
	{
		if (GInit) return;
		GInit = true;
		GC[0].Init(133.0, 2 * PI / 200.0, 2 * PI / Split, 0x0ce4u);
		GC[1].Init(31.0, 2 * PI / Split, 2 * PI / .7, 0x5ea2u);
		for (FCascade& C : GC)
			for (int32 i = 0; i < N * N; i++) { double Kx, Kz; C.K(i, Kx, Kz); C.Om[i] = FMath::Sqrt(G * FMath::Sqrt(Kx * Kx + Kz * Kz)); }
		for (TArray<FCx>& B : GBuf) B.SetNum(N * N);
	}

	// how much of the sea's height variance sits at wavelength Lam per unit of ln(k): a peak at the wind's wavelength,
	// a quick fall-off toward the long side and the k^-2 saturation tail toward the short side (so the ripples carry
	// slope more than height, as on a real sea)
	FORCEINLINE double Shape(double Lam, double Lp)
	{
		const double Q = Lam / Lp;
		return Q >= 1 ? FMath::Exp(-FMath::Square(FMath::Loge(Q)) / .25) : Q * Q;
	}

	FORCEINLINE float Bilerp(const TArray<float>& A, double U, double V)
	{
		const double Fu = FMath::Floor(U), Fv = FMath::Floor(V);
		const double Tu = U - Fu, Tv = V - Fv;
		const int32 I0 = int32(int64(Fu) & (N - 1)), J0 = int32(int64(Fv) & (N - 1));
		const int32 I1 = (I0 + 1) & (N - 1), J1 = (J0 + 1) & (N - 1);
		const float* D = A.GetData();
		const double Top = D[J0 * N + I0] + (D[J0 * N + I1] - D[J0 * N + I0]) * Tu;
		const double Bot = D[J1 * N + I0] + (D[J1 * N + I1] - D[J1 * N + I0]) * Tu;
		return float(Top + (Bot - Top) * Tv);
	}
}

namespace KeysOcean
{
	bool Enabled() { return GOn; }
	void SetEnabled(bool bOn) { GOn = bOn; }

	void Update(double T, double Rms, double PeakLen, double SeaDir, double Confusion, double Steep)
	{
		InitAll();
		const double Dt = GLastT < 0 ? 0 : FMath::Clamp(T - GLastT, 0.0, .25);
		GLastT = T;
		GT = T;
		const double Lp = FMath::Max(3.0, PeakLen);
		const double Cd = FMath::Cos(SeaDir), Sd = FMath::Sin(SeaDir);
		// 1) this frame's spectrum: shape x direction x the spectral cell, then one scale for the whole sea so its rms
		//    height is the sea state's
		double Sum2 = 0;
		for (FCascade& C : GC)
		{
			const double Cell = FMath::Square(2 * PI / C.L);
			for (int32 i = 0; i < N * N; i++)
			{
				double Kx, Kz; C.K(i, Kx, Kz);
				const double Kk = FMath::Sqrt(Kx * Kx + Kz * Kz);
				if (Kk < C.KMin || Kk >= C.KMax || Kk < 1e-6) { C.H0[i] = {0, 0}; continue; }
				const double Lam = 2 * PI / Kk;
				const double Cs = (Kx * Cd + Kz * Sd) / Kk;
				// running with the sea, a little wider for the short chop, from everywhere when confused
				const double Narrow = FMath::Pow(FMath::Max(0.0, Cs), Lam > Lp * .5 ? 4.0 : 2.0);
				const double Dir = FMath::Lerp(Narrow, .25 + .25 * FMath::Max(0.0, Cs), Confusion) + .02;
				// energy per ln k -> per spectral cell: / k^2 x the cell area
				const double E = Shape(Lam, Lp) * Dir / (Kk * Kk) * Cell;
				const double A = FMath::Sqrt(E);
				C.H0[i] = {C.Xi[i].R * A, C.Xi[i].I * A};
				Sum2 += A * A * (C.Xi[i].R * C.Xi[i].R + C.Xi[i].I * C.Xi[i].I);
			}
		}
		const double Scale = Sum2 > 1e-12 ? Rms / FMath::Sqrt(2 * Sum2) : 0;
		// sharper crests as the wind builds (the sideways pull toward each crest), more again in a confused sea
		const double Chop = FMath::Clamp(1.0 + 1.3 * Steep + .4 * Confusion, 0.0, 2.6);
		// how far a crest must be squeezed to break white: a light breeze barely caps, 20 knots caps all over
		const double FoamBias = FMath::Lerp(.35, .78, FMath::Clamp(Steep * 1.3, 0.0, 1.0));
		for (FCascade& C : GC)
		{
			// 2) each wave advanced by its own speed (deep water: w^2 = g k), packed two real fields per complex FFT
			for (int32 i = 0; i < N * N; i++)
			{
				const int32 n = i % N, m = i / N, Ni = ((N - m) & (N - 1)) * N + ((N - n) & (N - 1));
				double Kx, Kz; C.K(i, Kx, Kz);
				const double Kk = FMath::Sqrt(Kx * Kx + Kz * Kz);
				const double Ph = C.Om[i] * T;
				const FCx E{FMath::Cos(Ph), -FMath::Sin(Ph)};   // e^{-i w t}: each wave runs along its k
				const FCx A{C.H0[i].R * Scale, C.H0[i].I * Scale}, Bn = Conj({C.H0[Ni].R * Scale, C.H0[Ni].I * Scale});
				const FCx Ht = {Mul(A, E).R + Mul(Bn, Conj(E)).R, Mul(A, E).I + Mul(Bn, Conj(E)).I};
				const double Ik = Kk > 1e-9 ? 1 / Kk : 0;
				// D = i k/|k| h chop (points pulled toward the crests), S = i k h, dD/dx = -kx^2/|k| h chop ...
				const FCx Dx{-Ht.I * Kx * Ik * Chop, Ht.R * Kx * Ik * Chop}, Dz{-Ht.I * Kz * Ik * Chop, Ht.R * Kz * Ik * Chop};
				const FCx Sx{-Ht.I * Kx, Ht.R * Kx}, Sz{-Ht.I * Kz, Ht.R * Kz};
				const double Jxx = -Kx * Kx * Ik * Chop, Jzz = -Kz * Kz * Ik * Chop, Jxz = -Kx * Kz * Ik * Chop;
				// A + iB with A, B Hermitian -> a + i b
				GBuf[0][i] = {Ht.R - Dx.I, Ht.I + Dx.R};
				GBuf[1][i] = {Dz.R - Sx.I, Dz.I + Sx.R};
				GBuf[2][i] = {Sz.R - Ht.I * Jxx, Sz.I + Ht.R * Jxx};
				GBuf[3][i] = {Ht.R * Jzz - Ht.I * Jxz, Ht.I * Jzz + Ht.R * Jxz};
			}
			for (TArray<FCx>& B : GBuf) Fft2(B);
			const double Fade = Dt > 0 ? FMath::Exp(-Dt / 3.2) : 1;
			for (int32 i = 0; i < N * N; i++)
			{
				C.H[i] = float(GBuf[0][i].R); C.Dx[i] = float(GBuf[0][i].I);
				C.Dz[i] = float(GBuf[1][i].R); C.Sx[i] = float(GBuf[1][i].I);
				C.Sz[i] = float(GBuf[2][i].R);
				const double Jxx = GBuf[2][i].I, Jzz = GBuf[3][i].R, Jxz = GBuf[3][i].I;
				const double Jac = (1 + Jxx) * (1 + Jzz) - Jxz * Jxz;
				C.J[i] = float(Jac);
				// whitecaps: building while the crest is folding, fading over a few seconds after
				const double Now = FMath::Clamp((FoamBias - Jac) / .45, 0.0, 1.0);
				C.Fm[i] = float(FMath::Max(double(C.Fm[i]) * Fade, Now));
			}
		}
	}

	FString Stats()
	{
		FString S;
		for (int32 c = 0; c < 2; c++)
		{
			const FCascade& C = GC[c];
			double H2 = 0, Jmin = 9, Fsum = 0, Dmax = 0;
			for (int32 i = 0; i < N * N; i++) { H2 += C.H[i] * C.H[i]; Jmin = FMath::Min(Jmin, double(C.J[i])); Fsum += C.Fm[i]; Dmax = FMath::Max(Dmax, double(FMath::Abs(C.Dx[i]))); }
			S += FString::Printf(TEXT(" [%.0f m tile: rms %.2f m, min J %.2f, foam %.1f%%, max side %.2f m]"), C.L, FMath::Sqrt(H2 / (N * N)), Jmin, 100 * Fsum / (N * N), Dmax);
		}
		return S;
	}

	void FillTexels(int32 Tile, TArray<FLinearColor>& Out)
	{
		InitAll();
		const FCascade& C = GC[FMath::Clamp(Tile, 0, 1)];
		Out.SetNumUninitialized(N * N);
		for (int32 i = 0; i < N * N; i++) Out[i] = FLinearColor(C.Sx[i], C.Sz[i], C.Fm[i], C.H[i]);
	}
	double TileLen(int32 Tile) { InitAll(); return GC[FMath::Clamp(Tile, 0, 1)].L; }
	int32 GridN() { return N; }

	// the short chop goes to the water material's normals (pixel-sharp) instead of the mesh, which can't resolve it;
	// physics (MinLen 0) still feels it
	static FORCEINLINE double W1(double MinLen) { return MinLen < 1 ? 1 : 0; }
	static FORCEINLINE double W0(double MinLen) { return 1 - FMath::SmoothStep(14.0, 45.0, MinLen); }

	double Height(double X, double Z, double MinLen)
	{
		if (!GInit) return 0;
		double H = 0;
		const double A0 = W0(MinLen), A1 = W1(MinLen);
		if (A0 > 0) H += A0 * Bilerp(GC[0].H, X / GC[0].L * N, Z / GC[0].L * N);
		if (A1 > 0) H += A1 * Bilerp(GC[1].H, X / GC[1].L * N, Z / GC[1].L * N);
		return H;
	}

	FVector Displacement(double X, double Z, double MinLen)
	{
		if (!GInit) return FVector::ZeroVector;
		FVector D = FVector::ZeroVector;
		const double A[2] = {W0(MinLen), W1(MinLen)};
		for (int32 c = 0; c < 2; c++)
		{
			if (A[c] <= 0) continue;
			const double U = X / GC[c].L * N, V = Z / GC[c].L * N;
			D += A[c] * FVector(Bilerp(GC[c].Dx, U, V), Bilerp(GC[c].H, U, V), Bilerp(GC[c].Dz, U, V));
		}
		return D;
	}

	FVector2D Slope(double X, double Z)
	{
		if (!GInit) return FVector2D::ZeroVector;
		FVector2D S = FVector2D::ZeroVector;
		for (const FCascade& C : GC)
		{
			const double U = X / C.L * N, V = Z / C.L * N;
			S += FVector2D(Bilerp(C.Sx, U, V), Bilerp(C.Sz, U, V));
		}
		return S;
	}

	double Foam(double X, double Z)
	{
		if (!GInit) return 0;
		const double F0 = Bilerp(GC[0].Fm, X / GC[0].L * N, Z / GC[0].L * N);
		const double F1 = Bilerp(GC[1].Fm, X / GC[1].L * N, Z / GC[1].L * N);
		return FMath::Clamp(FMath::Max(F0, .55 * F1), 0.0, 1.0);
	}
}
