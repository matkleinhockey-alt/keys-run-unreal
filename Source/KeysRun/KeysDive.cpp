#include "KeysDive.h"
#include "KeysSea.h"
#include "KeysGeo.h"
#include "KeysData.h"

namespace
{
	double DClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	constexpr double KickFloor = .35, KickPeak = 4 * (1 - KickFloor);
}

namespace KeysDive
{
	double AtaAt(double Depth) { return 1 + Depth / 10; }
	double BuoyancyAccel(double Depth) { return DClamp(BuoySurfaceAccel * (1 - Depth / NeutralDepth), BuoyDriftUpAccel, BuoySurfaceAccel); }
	double AirBurnRate(double Depth, double Exertion) { return AtaAt(Depth) * (1 + DClamp(Exertion, 0, 1) * ExertionAirGain); }
	double KickPulse(double Phase) { const double S = FMath::Max(0.0, FMath::Sin(Phase * PI * 2)); return KickFloor + KickPeak * S * S; }

	FVector2D WaterVelocity(double X, double Y, double Z, double T, double FloorY)
	{
		const FVector2D Steady = KeysSea::SteadyCurrentAt(X, Z, T);
		const FVector2D Surge = KeysSea::SurgeAt(X, Z, T, FMath::Max(0.0, -Y));
		const double Hh = FMath::Max(0.0, Y - FloorY);
		const double S = DClamp(Hh / BoundaryH, 0, 1);
		const double Boundary = BoundaryMin + (1 - BoundaryMin) * S * S * (3 - 2 * S);
		return (Steady + Surge) * (CurrentCoupling * Boundary);
	}

	FState CreateState(double X, double Y, double Z, double Yaw, double Air)
	{
		FState S;
		S.X = X; S.Y = Y; S.Z = Z; S.Yaw = Yaw;
		S.Air = S.AirMaxV = Air;
		return S;
	}

	static void Integrate(FState& S, const FInput& In, double T, double Dt, bool bKicks, double SwimMul)
	{
		const double Cp = FMath::Cos(S.Pitch);
		const double FX = -FMath::Sin(S.Yaw) * Cp, FY = FMath::Sin(S.Pitch), FZ = -FMath::Cos(S.Yaw) * Cp;
		if (bKicks)
		{
			const double RX = FMath::Cos(S.Yaw), RZ = -FMath::Sin(S.Yaw);
			double KX = 0, KY = 0, KZ = 0;
			if (In.bFwd) { KX += FX; KY += FY; KZ += FZ; }
			if (In.bBack) { KX -= FX * BackKickFraction; KY -= FY * BackKickFraction; KZ -= FZ * BackKickFraction; }
			if (In.bRight) { KX += RX; KZ += RZ; }
			if (In.bLeft) { KX -= RX; KZ -= RZ; }
			const double Mag = FMath::Sqrt(KX * KX + KY * KY + KZ * KZ);
			if (Mag > 1) { KX /= Mag; KY /= Mag; KZ /= Mag; }
			const double SprintK = In.bSprint ? SprintMult : 1;
			const bool bSwim = Mag > 1e-6 || In.bAscend || In.bDescend;
			S.bKicking = bSwim;
			if (bSwim) S.KickPhase = FMath::Fmod(S.KickPhase + Dt / (In.bSprint ? KickPeriodSprint : KickPeriod), 1.0);
			else S.KickPhase = 0;
			const double Pulse = KickPulse(S.KickPhase);
			const double Sm = FMath::IsFinite(SwimMul) ? DClamp(SwimMul, .5, 3) : 1;
			const double Acc = SwimKickAccel * SprintK * Pulse * Sm;
			S.VX += KX * Acc * Dt; S.VY += KY * Acc * Dt; S.VZ += KZ * Acc * Dt;
			const double VAcc = VerticalKickAccel * SprintK * Pulse * Sm;
			if (In.bDescend && !In.bAscend) S.VY -= VAcc * Dt;
		}
		else { S.bKicking = false; S.KickPhase = 0; }

		if (S.Y < 0) S.VY += BuoyancyAccel(FMath::Max(0.0, -S.Y)) * Dt;
		else S.VY -= 9.8 * Dt;

		// drag on the velocity relative to the water, split along and across the body
		const double FloorHere = KeysSea::SeafloorHeightAt(S.X, S.Z);
		const FVector2D W = WaterVelocity(S.X, S.Y, S.Z, T, FloorHere);
		const double RVX = S.VX - W.X, RVY = S.VY, RVZ = S.VZ - W.Y;
		const double Par = RVX * FX + RVY * FY + RVZ * FZ;
		const double PX = RVX - Par * FX, PY = RVY - Par * FY, PZ = RVZ - Par * FZ;
		const double KPar = DragAxialLin + DragAxialQuad * FMath::Abs(Par);
		const double KPerp = DragLateralLin + DragLateralQuad * FMath::Sqrt(PX * PX + PY * PY + PZ * PZ);
		const double DPar = 1 / (1 + KPar * Dt), DPerpH = 1 / (1 + KPerp * Dt), DPerpV = 1 / (1 + KPerp * VerticalLateralDragScale * Dt);
		S.VX = W.X + Par * DPar * FX + PX * DPerpH;
		S.VY = Par * DPar * FY + PY * DPerpV;
		S.VZ = W.Y + Par * DPar * FZ + PZ * DPerpH;

		// the surface key always wins: a steady committed swim for the top, easing off in the last 1.5 m
		if (bKicks && In.bAscend && S.Y < 0)
		{
			const double Ease = DClamp(-S.Y / AscendEaseDepth, 0, 1);
			const double Target = (In.bSprint ? AscendSpeedSprint : AscendSpeed) * Ease;
			if (S.VY < Target) S.VY += (Target - S.VY) * FMath::Min(1.0, AscendResponse * Dt);
		}

		S.X += S.VX * Dt; S.Y += S.VY * Dt; S.Z += S.VZ * Dt;
		const double FloorY = KeysSea::SeafloorHeightAt(S.X, S.Z);
		if (S.Y < FloorY + DiverRadius)
		{
			S.Y = FloorY + DiverRadius;
			if (S.VY < 0) S.VY = 0;
			S.Events.Add(EEvent::GroundHit);
		}
		S.X = DClamp(S.X, KeysGeo::WBX0, KeysGeo::WBX1);
		S.Z = DClamp(S.Z, KeysGeo::WBZ0, KeysGeo::WBZ1);
	}

	static void Blackout(FState& S, EEvent Cause)
	{
		S.bBlackedOut = true;
		S.BlackoutT = 0;
		S.Air = 0;
		S.Events.Add(Cause);
	}

	void Step(FState& S, const FInput& In, double T, double Dt, double SwimMul)
	{
		S.Events.Reset();
		const bool bWasSub = S.Y < -SurfaceEps;
		if (S.bBlackedOut)
		{
			S.BlackoutT += Dt;
			Integrate(S, FInput(), T, Dt, false, SwimMul);
			return;
		}
		S.Yaw = In.LookYaw;
		S.Pitch = DClamp(In.LookPitch, -1.3, 1.3);
		const double Depth = S.Depth();
		S.bNarcosis = Depth >= NarcosisDepth;
		const bool bKick = In.bFwd || In.bBack || In.bLeft || In.bRight || In.bAscend || In.bDescend;
		const double TargetEx = In.bSprint ? 1 : bKick ? .45 : 0;
		const double ExRate = TargetEx > S.Exertion ? ExertionRiseRate : ExertionFallRate;
		S.Exertion += (TargetEx - S.Exertion) * FMath::Min(1.0, Dt * ExRate);
		const bool bAtSurface = Depth <= SurfaceEps && S.VY >= -.05;
		if (bAtSurface) S.Air = FMath::Min(S.AirMaxV, S.Air + (S.AirMaxV / 3) * Dt);
		else S.Air = FMath::Max(0.0, S.Air - AirBurnRate(Depth, S.Exertion) * Dt);

		Integrate(S, In, T, Dt, true, SwimMul);

		const double NewDepth = S.Depth();
		if (S.Air <= 0) Blackout(S, EEvent::BlackoutAir);
		else if (NewDepth > SurfaceEps && NewDepth < ShallowBlackoutDepth && S.Air < AirMax * ShallowBlackoutAirFraction && S.VY > ShallowBlackoutMinAscent)
			Blackout(S, EEvent::BlackoutShallow);
		const bool bIsSub = S.Y < -SurfaceEps;
		if (bIsSub && !bWasSub) S.Events.Add(EEvent::Submerged);
		else if (!bIsSub && bWasSub) S.Events.Add(EEvent::Surfaced);
	}

	// ================================================================ spear
	double DragKFor(double Range) { return (.4 * SpearSpeed) / Range; }
	double DistAtFlightTime(double T, double Range) { const double K = DragKFor(Range); return (SpearSpeed / K) * (1 - FMath::Exp(-K * T)); }
	double TimeToDistance(double Dist, double Range)
	{
		const double K = DragKFor(Range), MaxD = SpearSpeed / K;
		return -FMath::Loge(1 - DClamp(Dist / MaxD, 0, .999)) / K;
	}
	double ShaftSpeedAtDist(double Dist, double Range) { return FMath::Max(0.0, SpearSpeed - DragKFor(Range) * Dist); }
	double HoldChance(double ImpactSpeed) { const double F = DClamp(ImpactSpeed / SpearSpeed, 0, 1); return DClamp((F - .25) / .6, 0, 1) * .95; }
	double DrawRateFor(double Speed) { const double K = DClamp(Speed / SpearDrawSwimSpeed, 0, 1); return 1 - (1 - SpearDrawMinRate) * K; }

	void StepReload(FGun& G, double Dt, bool bDrawing, double Speed, double DrawMul)
	{
		if (G.Draw >= 1 && G.ReloadT <= 0) return;
		if (G.ReloadT > 0) { G.ReloadT = FMath::Max(0.0, G.ReloadT - Dt); return; }
		if (bDrawing) G.Draw = FMath::Min(1.0, G.Draw + (Dt / (SpearDrawTime * DClamp(DrawMul, .3, 1.5))) * DrawRateFor(Speed));
		else
		{
			const double Notch = FMath::FloorToDouble(G.Draw * SpearDrawNotches + 1e-9) / SpearDrawNotches;
			G.Draw = FMath::Max(Notch, G.Draw - SpearDrawSlip * Dt);
		}
	}

	FShot Fire(const FVector& Origin, const FVector& Dir, double Range)
	{
		FShot S;
		S.O = Origin;
		S.D = Dir.GetSafeNormal();
		if (S.D.IsNearlyZero()) S.D = FVector(0, 0, -1);
		S.Range = Range;
		S.bAlive = true;
		return S;
	}

	FVector AssistAim(const FVector& Origin, const FVector& Dir, const TArray<FTarget>& Targets, int32& OutTargetId, double Strength, double Range, const FVector* EyeP)
	{
		OutTargetId = -1;
		const double Cone = AssistCone;
		Strength = DClamp(Strength, 0, 1);
		const FVector Eye = EyeP ? *EyeP : Origin;
		const FVector U = Dir.GetSafeNormal();
		double BestAng = 1e9;
		const FTarget* Best = nullptr;
		FVector BestC = FVector::ZeroVector;
		for (const FTarget& Tg : Targets)
		{
			if (!Tg.bCatchable) continue;
			const FVector C = (Tg.A + Tg.B) * .5;
			const double D0 = (C - Origin).Size();
			if (D0 < .3 || D0 > Range) continue;
			FVector Aim = C;
			if (!Tg.Vel.IsNearlyZero())
				for (int32 i = 0; i < 2; i++)
				{
					const double Tof = TimeToDistance(FMath::Min((Aim - Origin).Size(), Range), Range);
					Aim = C + Tg.Vel * Tof;
				}
			const FVector E = Aim - Eye;
			const double El = FMath::Max(1e-9, E.Size());
			const double Ang = FMath::Acos(DClamp(FVector::DotProduct(E, U) / El, -1, 1));
			if (Ang > Cone) continue;
			if (Ang < BestAng) { BestAng = Ang; Best = &Tg; BestC = C; }
		}
		if (!Best) return U;
		OutTargetId = Best->Id;
		// adhesion toward where the fish is (blended), then the lead for the shaft's flight time (in full)
		const double D0b = (BestC - Eye).Size();
		const FVector RP = Eye + U * D0b;
		const FVector Adh = RP + (BestC - RP) * Strength;
		FVector Lead = Adh;
		if (!Best->Vel.IsNearlyZero())
			for (int32 i = 0; i < 2; i++)
			{
				const double Tof = TimeToDistance(FMath::Min((Lead - Origin).Size(), Range), Range);
				Lead = Adh + Best->Vel * Tof;
			}
		const FVector Bv = Lead - Eye;
		const double Bl = FMath::Max(1e-9, Bv.Size());
		FVector A = Bv / Bl;
		const double Dot = DClamp(FVector::DotProduct(A, U), -1, 1);
		if (FMath::Acos(Dot) > Cone)
		{
			FVector P3 = (A - U * Dot).GetSafeNormal();
			A = U * FMath::Cos(Cone) + P3 * FMath::Sin(Cone);
		}
		A = A.GetSafeNormal();
		// point the muzzle at where the eye ray reaches at the target's distance (parallax, applied in full)
		const FVector P2 = Eye + A * Bl - Origin;
		return P2.GetSafeNormal();
	}

	double ClosestDistSqSegSeg(const FVector& P1, const FVector& P2, const FVector& Q1, const FVector& Q2, double& OutS)
	{
		const FVector D1 = P2 - P1, D2 = Q2 - Q1, R = P1 - Q1;
		const double A = D1.SizeSquared(), E = D2.SizeSquared(), F = FVector::DotProduct(D2, R);
		constexpr double Eps = 1e-12;
		double S, T;
		if (A <= Eps && E <= Eps) { S = 0; T = 0; }
		else if (A <= Eps) { S = 0; T = DClamp(F / E, 0, 1); }
		else
		{
			const double C = FVector::DotProduct(D1, R);
			if (E <= Eps) { T = 0; S = DClamp(-C / A, 0, 1); }
			else
			{
				const double B = FVector::DotProduct(D1, D2), Denom = A * E - B * B;
				S = Denom > 1e-10 * A * E ? DClamp((B * F - C * E) / Denom, 0, 1) : 0;
				T = (B * S + F) / E;
				if (T < 0) { T = 0; S = DClamp(-C / A, 0, 1); }
				else if (T > 1) { T = 1; S = DClamp((B - C) / A, 0, 1); }
			}
		}
		OutS = S;
		return ((P1 + D1 * S) - (Q1 + D2 * T)).SizeSquared();
	}

	bool StepSpear(FShot& Shot, const TArray<FTarget>& Targets, double Dt, FHit& OutHit)
	{
		if (!Shot.bAlive) return false;
		const double NewT = Shot.T + Dt;
		const double NewDist = FMath::Min(DistAtFlightTime(NewT, Shot.Range), Shot.Range);
		const double Travel = NewDist - Shot.Dist;
		const FVector P1 = Shot.At(Shot.Dist), P2 = Shot.At(NewDist);
		const FTarget* Best = nullptr;
		double BestS = 2;
		for (const FTarget& Tg : Targets)
		{
			if (!Tg.bCatchable) continue;
			double S;
			const double D2 = ClosestDistSqSegSeg(P1, P2, Tg.A, Tg.B, S);
			if (D2 <= Tg.Radius * Tg.Radius && S < BestS) { Best = &Tg; BestS = S; }
		}
		if (Best)
		{
			OutHit.Id = Best->Id;
			OutHit.AtDist = Shot.Dist + Travel * BestS;
			OutHit.Point = Shot.At(OutHit.AtDist);
			OutHit.ImpactSpeed = ShaftSpeedAtDist(OutHit.AtDist, Shot.Range);
			OutHit.bHeld = FMath::FRand() < HoldChance(OutHit.ImpactSpeed);
			Shot.Dist = OutHit.AtDist; Shot.T = NewT; Shot.bAlive = false;
			return true;
		}
		Shot.Dist = NewDist; Shot.T = NewT;
		Shot.bAlive = NewDist < Shot.Range - 1e-9;
		return false;
	}

	FFightParams FightParamsFor(const FString& Key, double Weight)
	{
		FFightParams P;
		P.Key = Key; P.Weight = Weight;
		if (const KeysData::FSpecies* S = KeysData::Species(Key))
		{
			const double SizeT = DClamp((Weight - S->Min) / FMath::Max(1e-6, S->Max - S->Min), 0, 1);
			P.Str = S->Str * (.8 + .55 * SizeT);
		}
		P.TetherMax = FMath::Max(4.0, SpearTetherM);
		return P;
	}

	FFight StartFight(double FishX, double FishZ) { FFight F; F.X = FishX; F.Z = FishZ; return F; }

	void StepFight(FFight& F, bool bHauling, const FFightParams& P, double DiverX, double DiverZ, double Dt)
	{
		if (F.Outcome != EFight::Fighting) return;
		const double Str = P.Str;
		F.ThrashT -= Dt;
		if (F.ThrashT <= 0)
		{
			F.ThrashT = .25 + FMath::FRand() * .5 * (1.2 - .5 * F.Stam);
			const double A = FMath::FRand() * PI * 2, R = Str * (1.5 + FMath::FRand());
			F.X += FMath::Cos(A) * R * Dt * 4;
			F.Z += FMath::Sin(A) * R * Dt * 4;
		}
		const double DX0 = F.X - DiverX, DZ0 = F.Z - DiverZ, Dist0 = FMath::Max(1e-9, FMath::Sqrt(DX0 * DX0 + DZ0 * DZ0));
		if (Dist0 > P.TetherMax) { F.X = DiverX + DX0 / Dist0 * P.TetherMax; F.Z = DiverZ + DZ0 / Dist0 * P.TetherMax; }
		if (bHauling) F.Tension = DClamp(F.Tension + (.3 + Str * .5) * Dt, 0, 1.2);
		else F.Tension += (.25 - F.Tension) * FMath::Min(1.0, Dt * 1.5);
		F.Stam = FMath::Max(0.0, F.Stam - Dt * (.12 + .18 * F.Tension) / FMath::Sqrt(Str));
		if (F.Tension >= 1.1)
		{
			F.OverT += Dt;
			if (F.OverT > 1) { F.Outcome = EFight::TornFree; return; }
		}
		else F.OverT = FMath::Max(0.0, F.OverT - Dt);
		if (bHauling)
		{
			const double DX = F.X - DiverX, DZ = F.Z - DiverZ, D = FMath::Max(1e-9, FMath::Sqrt(DX * DX + DZ * DZ));
			const double R = 2 + Str;
			F.X -= DX / D * R * Dt;
			F.Z -= DZ / D * R * Dt;
		}
		F.Dist = FMath::Sqrt(FMath::Square(F.X - DiverX) + FMath::Square(F.Z - DiverZ));
		if (F.Stam <= 0 && F.Dist < 1.2) F.Outcome = EFight::Landed;
	}
}
