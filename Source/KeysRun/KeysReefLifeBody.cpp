// Reef life, the parts with no state: the species registry, what the bottom is at a point, and the fish body builder.
#include "KeysReefLife.h"
#include "KeysMesh.h"
#include "KeysGeo.h"
#include "KeysSea.h"

namespace KeysReefLife
{
	namespace
	{
		// filled by FAuto while the module's statics are constructed: plain data, so the order does not matter
		constexpr int32 MaxMakers = 256;
		FMake GMakers[MaxMakers];
		int32 GNumMakers = 0;

		double Smooth01(double T) { T = FMath::Clamp(T, 0.0, 1.0); return T * T * (3 - 2 * T); }

		// the body's half-depth and half-width along its length, as fractions of the greatest
		double ProfH(const FBody& B, double U)
		{
			if (U < B.Peak) return B.NoseR + (1 - B.NoseR) * FMath::Pow(FMath::Sin(U / B.Peak * HALF_PI), FMath::Lerp(1.0, .42, B.Blunt));
			return FMath::Lerp(1.0, B.Ped, Smooth01((U - B.Peak) / (1 - B.Peak)));
		}
		double ProfW(const FBody& B, double U)
		{
			const double Pk = B.Peak * .85;
			if (U < Pk) return .4 + .6 * FMath::Pow(FMath::Sin(U / Pk * HALF_PI), .6);
			return FMath::Lerp(1.0, B.PedW, Smooth01((U - Pk) / (1 - Pk)));
		}
		double BodyX(const FBody& B, double U) { return .5 - U * (1 - B.TailLen); }
		FVector BodyPt(const FBody& B, double U, double Theta)
		{
			const double S = FMath::Sin(Theta), C = FMath::Cos(Theta);
			const double H = B.HalfH * ProfH(B, U) * (S > 0 ? 1 + B.Arch : 1 - B.Arch);
			return FVector(BodyX(B, U), B.HalfW * ProfW(B, U) * C, H * S);
		}
		FVector BodyNormal(const FBody& B, double U, double Theta)
		{
			const double E = .004;
			const FVector Du = BodyPt(B, FMath::Min(1.0, U + E), Theta) - BodyPt(B, FMath::Max(0.0, U - E), Theta);
			const FVector Dt = BodyPt(B, U, Theta + .02) - BodyPt(B, U, Theta - .02);
			FVector N = FVector::CrossProduct(Dt, Du).GetSafeNormal();
			if (N.IsNearlyZero()) N = FVector(0, FMath::Cos(Theta), FMath::Sin(Theta));
			if (N.Y * FMath::Cos(Theta) + N.Z * FMath::Sin(Theta) < 0) N = -N;
			return N;
		}
		// a flat sheet of NU x NV points (P[v * NU + u]) with one normal
		void AddSheet(FKeysMesh& M, const TArray<FVector>& P, const TArray<FLinearColor>& C, int32 NU, int32 NV, const FVector& Nrm)
		{
			const int32 I0 = M.V.Num();
			for (int32 i = 0; i < P.Num(); i++) M.AddVert(P[i], Nrm, C[i]);
			for (int32 v = 0; v + 1 < NV; v++)
				for (int32 u = 0; u + 1 < NU; u++)
				{
					const int32 A = I0 + v * NU + u, Bq = A + 1, Cq = A + NU + 1, D = A + NU;
					M.T.Append({A, Bq, Cq, A, Cq, D});
				}
		}
	}

	void Register(FMake Make) { if (Make && GNumMakers < MaxMakers) GMakers[GNumMakers++] = Make; }

	const TArray<FSpecies>& Registry()
	{
		static TArray<FSpecies> R;
		static bool bBuilt = false;
		if (bBuilt) return R;
		bBuilt = true;
		for (int32 i = 0; i < GNumMakers; i++)
		{
			FSpecies S = GMakers[i]();
			if (S.Key.IsEmpty() || !S.Phases.Num() || !S.Mixes.Num() || !S.Paint) { UE_LOG(LogTemp, Warning, TEXT("KRReef: species '%s' left out (it needs a Key, a phase, a mix and a Paint)"), *S.Key); continue; }
			if (R.ContainsByPredicate([&](const FSpecies& O) { return O.Key == S.Key; })) { UE_LOG(LogTemp, Warning, TEXT("KRReef: species key '%s' registered twice; the second is left out"), *S.Key); continue; }
			R.Add(MoveTemp(S));
		}
		R.Sort([](const FSpecies& A, const FSpecies& B) { return A.Key < B.Key; });
		return R;
	}

	int32 FindSpecies(const FString& Key)
	{
		const TArray<FSpecies>& R = Registry();
		for (int32 i = 0; i < R.Num(); i++) if (R[i].Key == Key) return i;
		return -1;
	}

	const TCHAR* ZoneName(uint32 Zone)
	{
		switch (Zone)
		{
		case ZCrest: return TEXT("crest");
		case ZTop: return TEXT("reef top");
		case ZSpur: return TEXT("spur top");
		case ZPatch: return TEXT("patch reef");
		case ZFore: return TEXT("fore reef");
		case ZWall: return TEXT("deep wall");
		case ZSand: return TEXT("sand");
		case ZGrass: return TEXT("seagrass");
		case ZHard: return TEXT("hard bottom");
		}
		return TEXT("none");
	}

	FHabitat HabitatAt(double X, double Z)
	{
		using namespace KeysGeo;
		FHabitat H;
		if (LandH(X, Z) >= 0) return H;
		const double D = DepthFast(X, Z);
		H.FloorY = KeysSea::SeafloorHeightAt(X, Z);
		H.Depth = FMath::Max(0.0, -H.FloorY);
		if (D < .3) return H;
		// never in a canal, a way cut out of one, a mangrove creek or under the mangroves
		if (CanalDist(X, Z) < 8 || KeysSea::DredgedAt(X, Z) > .2 || MangroveNear(X, Z) > .2 || CreekWater(X, Z) > .3 || CreekDist(X, Z) < 4) return H;
		const KeysSea::FBottom B = KeysSea::BottomAt(X, Z, D);
		H.Reef = B.Reef; H.Hard = B.Hard; H.Grass = B.Grass;
		H.Relief = KeysSea::ReliefAt(X, Z, D);
		if (B.Reef >= .35)
		{
			// the reef line lies past 1250 m "south of the chain" in the original map's terms (KeysReef's ReefZone);
			// reef inshore of that is a patch reef
			if (DZAt(X, Z) <= 1250) H.Zone = ZPatch;
			else if (H.Depth < 2.6) H.Zone = ZCrest;
			else if (H.Depth < 9) H.Zone = H.Relief > .35 ? ZSpur : ZTop;
			else if (H.Depth < 16) H.Zone = H.Relief > .35 ? ZSpur : ZFore;
			else H.Zone = ZWall;
		}
		else if (B.Grass >= .35) H.Zone = ZGrass;
		else if (B.Hard >= .35) H.Zone = ZHard;
		else H.Zone = ZSand;
		return H;
	}

	void BuildBody(FKeysMesh& M, const FBody& B, int32 Lod, bool bWithPectorals, const TFunction<FLinearColor(const FPaint&)>& Paint)
	{
		Lod = FMath::Clamp(Lod, 0, 2);
		const int32 NS = Lod == 0 ? 24 : Lod == 1 ? 10 : 5, NA = Lod == 0 ? 16 : Lod == 1 ? 8 : 4;
		auto Col = [&](EPart Part, double U, double V, int32 I, int32 J, int32 Side) { FPaint P; P.Part = Part; P.U = U; P.V = V; P.I = I; P.J = J; P.Side = Side; P.Lod = Lod; return Paint(P); };
		// ---- the body: rings of NA points from the snout to the tail base
		M.Surf = SurfFish;
		const int32 I0 = M.V.Num();
		TArray<double> Us;
		for (int32 i = 0; i <= NS; i++) Us.Add(.012 + .988 * FMath::Pow(double(i) / NS, 1.3));
		for (int32 i = 0; i <= NS; i++)
			for (int32 j = 0; j < NA; j++)
			{
				const double Th = HALF_PI + 2 * PI * j / NA;
				M.AddVert(BodyPt(B, Us[i], Th), BodyNormal(B, Us[i], Th), Col(PBody, Us[i], FMath::Sin(Th), i, j, FMath::Cos(Th) >= 0 ? 1 : -1));
			}
		for (int32 i = 0; i < NS; i++)
			for (int32 j = 0; j < NA; j++)
			{
				const int32 A = I0 + i * NA + j, Bq = I0 + i * NA + (j + 1) % NA, Cq = I0 + (i + 1) * NA + (j + 1) % NA, D = I0 + (i + 1) * NA + j;
				// the diagonal alternates, so a colour set on every other vertex reads as scales, not as stripes
				if ((i + j) & 1) M.T.Append({A, Bq, Cq, A, Cq, D}); else M.T.Append({A, Bq, D, Bq, Cq, D});
			}
		// the snout and the tail base closed
		const int32 Nose = M.AddVert(FVector(BodyX(B, 0) + .006, 0, 0), FVector(1, 0, 0), Col(PBody, 0, 0, 0, 0, 1));
		const int32 Butt = M.AddVert(FVector(BodyX(B, 1), 0, 0), FVector(-1, 0, 0), Col(PBody, 1, 0, NS, 0, 1));
		for (int32 j = 0; j < NA; j++)
		{
			M.T.Append({Nose, I0 + j, I0 + (j + 1) % NA});
			M.T.Append({Butt, I0 + NS * NA + j, I0 + NS * NA + (j + 1) % NA});
		}
		// ---- fins: flat sheets on the centre line
		M.Surf = SurfFin;
		const FVector FinN(0, 1, 0);
		{
			const int32 N = Lod == 0 ? 9 : Lod == 1 ? 4 : 2;
			for (int32 Fin = 0; Fin < 2; Fin++)
			{
				const double U0 = Fin ? B.An0 : B.Dor0, U1 = Fin ? B.An1 : B.Dor1, Ht = Fin ? B.AnH : B.DorH, Sg = Fin ? -1 : 1, Trail = Fin ? B.AnTrail : B.DorTrail;
				if (Ht <= 0 || U1 <= U0) continue;
				TArray<FVector> P;
				TArray<FLinearColor> C;
				P.SetNum(N * 2); C.SetNum(N * 2);
				for (int32 k = 0; k < N; k++)
				{
					const double T = double(k) / (N - 1), U = FMath::Lerp(U0, U1, T);
					// a trailing fin: low over the spines in front, tallest at the rear, the tip swept back past the base
					const double Zb = BodyPt(B, U, Sg * HALF_PI).Z * .93, Hp = Trail > 0 ? Ht * (.3 + .7 * FMath::Pow(T, 1.5)) : Ht * FMath::Pow(FMath::Sin(PI * (.05 + .9 * T)), .4);
					P[k] = FVector(BodyX(B, U), 0, Zb);
					P[N + k] = FVector(BodyX(B, U) - Hp * .35 - Trail * T * T * T, 0, Zb + Sg * Hp);
					C[k] = Col(Fin ? PAnal : PDorsal, T, 0, k, 0, 1);
					C[N + k] = Col(Fin ? PAnal : PDorsal, T, 1, k, 1, 1);
				}
				AddSheet(M, P, C, N, 2, FinN);
			}
		}
		{
			// the tail fin
			const int32 NU = Lod == 0 ? 5 : 2, NV = Lod == 0 ? 7 : 3;
			const double Xp = BodyX(B, 1) + .01, R0 = B.HalfH * B.Ped * .95;
			TArray<FVector> P;
			TArray<FLinearColor> C;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const double Sx = double(u) / (NU - 1), Vv = -1 + 2.0 * v / (NV - 1);
					const double Edge = -.5 + (B.TailFork >= 0 ? B.TailFork * B.TailLen * (1 - Vv * Vv) : -B.TailFork * B.TailLen * Vv * Vv);
					P.Add(FVector(FMath::Lerp(Xp, Edge, Sx), 0, Vv * FMath::Lerp(R0, B.TailH * .5, FMath::Pow(Sx, .8))));
					C.Add(Col(PTail, Sx, Vv, u, v, 1));
				}
			AddSheet(M, P, C, NU, NV, FinN);
		}
		if (Lod < 2)
		{
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				// pelvic fins, and the pectorals when they are not drawn as rowing fins
				const double Th = Sd > 0 ? FMath::Asin(FMath::Clamp(B.PecV, -1.0, 1.0)) : PI - FMath::Asin(FMath::Clamp(B.PecV, -1.0, 1.0));
				if (bWithPectorals && B.PecLen > 0)
				{
					const FVector Rt = BodyPt(B, B.PecU, Th) + FVector(0, Sd * .003, 0), Dir(-.84, Sd * .5, -.2);
					TArray<FVector> P = {Rt - FVector(0, 0, B.PecW * .25), Rt + Dir * B.PecLen * .85 - FVector(0, 0, B.PecW * .5), Rt + FVector(0, 0, B.PecW * .25), Rt + Dir * B.PecLen + FVector(0, 0, B.PecW * .5)};
					TArray<FLinearColor> C = {Col(PPectoral, 0, -1, 0, 0, Sd), Col(PPectoral, 1, -1, 1, 0, Sd), Col(PPectoral, 0, 1, 0, 1, Sd), Col(PPectoral, 1, 1, 1, 1, Sd)};
					AddSheet(M, P, C, 2, 2, FVector(.5, Sd * .84, 0));
				}
				if (Lod == 0 && B.PelLen > 0)
				{
					const FVector Rt = BodyPt(B, B.PelU, -HALF_PI + Sd * .35);
					TArray<FVector> P = {Rt, Rt - FVector(B.PelLen * .35, 0, 0), Rt + FVector(-B.PelLen * .5, Sd * .012, -B.PelLen * .45), Rt + FVector(-B.PelLen, Sd * .02, -B.PelLen * .4)};
					TArray<FLinearColor> C = {Col(PPelvic, 0, 0, 0, 0, Sd), Col(PPelvic, 0, 1, 0, 1, Sd), Col(PPelvic, 1, 0, 1, 0, Sd), Col(PPelvic, 1, 1, 1, 1, Sd)};
					AddSheet(M, P, C, 2, 2, FVector(0, Sd, 0));
				}
				// the eye: a pupil and its ring, a hair proud of the head
				if (B.EyeR > 0)
				{
					M.Surf = SurfEye;
					const double Te = Sd > 0 ? FMath::Asin(FMath::Clamp(B.EyeV, -1.0, 1.0)) : PI - FMath::Asin(FMath::Clamp(B.EyeV, -1.0, 1.0));
					const FVector N = BodyNormal(B, B.EyeU, Te), Ctr = BodyPt(B, B.EyeU, Te) + N * .004;
					FVector T1 = FVector::CrossProduct(N, FVector(0, 0, 1)).GetSafeNormal();
					if (T1.IsNearlyZero()) T1 = FVector(1, 0, 0);
					const FVector T2 = FVector::CrossProduct(N, T1);
					const int32 NE = Lod == 0 ? 8 : 4, E0 = M.AddVert(Ctr + N * .003, N, Col(PEye, 0, 0, 0, 0, Sd));
					for (int32 Ring = 0; Ring < 2; Ring++)
						for (int32 k = 0; k < NE; k++)
						{
							const double A = 2 * PI * k / NE, Rr = B.EyeR * (Ring ? 1.0 : .55);
							M.AddVert(Ctr + (T1 * FMath::Cos(A) + T2 * FMath::Sin(A)) * Rr + N * (Ring ? 0.0 : .002), N, Col(PEye, Ring ? 1.0 : .55, 0, Ring + 1, k, Sd));
						}
					for (int32 k = 0; k < NE; k++)
					{
						const int32 A = E0 + 1 + k, Bq = E0 + 1 + (k + 1) % NE;
						M.T.Append({E0, A, Bq, A, A + NE, Bq + NE, A, Bq + NE, Bq});
					}
					M.Surf = SurfFin;
				}
			}
		}
		M.Surf = 0;
	}

	void BuildPectoral(FKeysMesh& M, const FBody& B, const TFunction<FLinearColor(const FPaint&)>& Paint)
	{
		// a rounded paddle in the x-z plane, root at the origin, pointing aft; the upper rays the longest
		M.Surf = SurfFin;
		const double Ss[4] = {0, .35, .72, 1};
		TArray<FVector> P;
		TArray<FLinearColor> C;
		for (int32 v = 0; v < 3; v++)
			for (int32 u = 0; u < 4; u++)
			{
				const double S = Ss[u], Vv = v - 1.0;
				double W = B.PecW * (.22 + .78 * FMath::Sin(HALF_PI * FMath::Min(1.0, S / .72)));
				if (S > .72) W *= 1 - .55 * FMath::Square((S - .72) / .28);
				P.Add(FVector(-S * B.PecLen * (1 + .12 * Vv), 0, Vv * W * .5 + S * B.PecLen * .1));
				FPaint At; At.Part = PPectoral; At.U = S; At.V = Vv; At.I = u; At.J = v;
				C.Add(Paint(At));
			}
		AddSheet(M, P, C, 4, 3, FVector(0, 1, 0));
		M.Surf = 0;
	}
}
