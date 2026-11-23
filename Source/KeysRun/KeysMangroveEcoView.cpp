// Seeing the mangrove ecosystem from the boat (part of KeysMangroveEco.cpp):
//  - the closer look: idling or stopped near the animals of a mangrove shore or a rookery key, the chase camera comes
//    in, down and round to the far side of the boat from them and the lens closes up, without being asked; it lets go
//    as she gathers way, when a line goes out, and the moment the player moves the camera himself. The rule is here;
//    AKeysBoatPawn::UpdateCamera only asks for it (CloserLook) and UpdateJumpCam for the lens (CloserFov).
//  - the measure: how tall each bird is on the screen, in pixels, from the player's own view (kr.eco px).
#include "KeysMangroveEcoInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysBoatPawn.h"
#include "KeysTrim.h"
#include "KeysDocks.h"
#include "KeysData.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace KeysMangroveEco
{
	namespace
	{
		constexpr double EngageMs = 1.55, ReleaseMs = 2.6;   // about 3 kn to come in, 5 kn to let go
		struct FLookIn
		{
			double K = 0;                 // 0 the plain chase view .. 1 right in
			bool bOn = false, bHeld = false, bOff = false, bHave = false, bHaveLast = false, bBehind = false;
			double SlowT = 0, PickT = 0, UserT = 1e9, QuietT = 0, Aw = 0, SideDist = 12, A = 0, Sign = 1, Score = 0, Across = 30, Fov = 88, OnT = 0;
			double LastYaw = 0, LastPitch = 0, LastZoom = 1, LastSp = 0;
			uint64 Frame = 0;
			FVector Tgt = FVector::ZeroVector, Look = FVector::ZeroVector;
			FString What, Want;   // Want (a test's): the kind the look is told to prefer
			int32 Rook = -1, Ons = 0, Offs = 0, Shower = 0;
		};
		FLookIn GL;

		struct FCand { FVector P; double W; int32 Kind; bool bSeen; };

		// what is worth looking at from here: a rookery key, else the best knot of animals within reach of the eye
		void Pick(FEco& E, const FVector2D& Boat, FLookIn& L)
		{
			L.bHave = false;
			double Best = 0;
			FVector BestP = FVector::ZeroVector;
			FString BestWhat;
			int32 BestRook = -1;
			double BestAcross = 30;
			const TArray<FRookery>& Rk = Rookeries();
			for (int32 r = 0; r < Rk.Num(); r++)
			{
				const double D = FVector2D::Distance(Rk[r].C, Boat);
				if (D > Rk[r].R + 230 || D < Rk[r].R * .6) continue;
				int32 N = 0;
				double Y = 0;
				for (TPair<int64, FTileE*>& T : E.Tiles)
					for (const FBird& B : T.Value->Birds) if (B.Rook == r && B.State != BAway) { N++; Y += B.P.Y; }
				if (N < 4) continue;
				const double Sc = 2.0 + N * .02 + (L.Rook == r ? 1 : 0);
				if (Sc > Best) { Best = Sc; BestP = FVector(Rk[r].C.X, Y / N - .5, Rk[r].C.Y); BestRook = r; BestAcross = Rk[r].R * 2.3; BestWhat = FString::Printf(TEXT("rookery %d (%d birds at home)"), r, N); }
			}
			// (bait going up in a shower within sight takes the eye for a few seconds: the birds are coming to it)
			for (const FShower& Sh : E.Showers)
			{
				const double D = FVector2D::Distance(Sh.P, Boat);
				if (E.Now - Sh.T > 6 || D > 80 || D < 5 || Best >= 5) continue;
				Best = 5; BestP = FVector(Sh.P.X, E.Sea + .4, Sh.P.Y); BestRook = -1; BestAcross = 26; BestWhat = TEXT("a shower of bait");
			}
			TArray<FCand> C;
			auto Add = [&](const FVector& P, double W, int32 Kind, const FVector2D& Out)
			{
				const FVector2D P2(P.X, P.Z);
				const double D = FVector2D::Distance(P2, Boat);
				if (D > (Kind == Osprey ? 160 : 95) || D < 3) return;
				// (what stands on the boat's side of its trees counts for more than what is behind them)
				const bool bSeen = FVector2D::DotProduct(Out, Boat - P2) > 0;
				// (the near ones count for much more: a kingfisher 25 m off is worth more than a heron at 60)
				C.Add({P, W * (bSeen ? 1 : .3) / (1 + FMath::Square(D / 22)) * (!L.Want.IsEmpty() && FString(KindName(Kind)).Contains(L.Want) ? 25 : 1), Kind, bSeen});
			};
			for (TPair<int64, FTileE*>& T : E.Tiles)
			{
				if (FMath::Abs(T.Value->C.X - Boat.X) > 400 || FMath::Abs(T.Value->C.Y - Boat.Y) > 400) continue;
				for (const FBird& B : T.Value->Birds)
				{
					// (not the ones on the wing: the eye is not dragged about after them. A test may ask for a tern or an osprey.)
					const bool bAir = B.State == BFly || B.State == BPatrol || B.State == BHover || B.State == BDive;
					if (B.Rook >= 0 || B.State == BAway || (bAir && (L.Want.IsEmpty() || !FString(KindName(B.Kind)).Contains(L.Want)))) continue;
					Add(B.P + FVector(0, .5, 0), B.Kind == Kingfisher || B.Kind == GreenHeron || B.Kind == NightHeron ? .8 : 1, B.Kind, B.Out);
				}
				for (const FSchoolE& S : T.Value->Schools) if (S.bAwake) Add(FVector(S.C.X, E.Sea, S.C.Y), .35, S.Kind, S.Out);
				for (const FGroup& G : T.Value->Groups) if (G.bActive && G.Kind != Jelly && G.Kind != NurseShark) Add(FVector(G.C.X, E.Sea, G.C.Y), .3, G.Kind, G.Out);
				for (const FCrabs& Cr : T.Value->Crabs)
					if (!Cr.bGone && Cr.Hide < .5 && FVector2D::Distance(Cr.C, Boat) < 16) Add(FVector(Cr.C.X, E.Sea + .4, Cr.C.Y), .6, Cr.Kind, Cr.Out);
			}
			for (int32 i = 0; i < C.Num(); i++)
			{
				// (a knot is gathered round a bird or crabs: fish alone do not hold the eye, unless a test asks for them)
				const bool bAsked = !L.Want.IsEmpty() && FString(KindName(C[i].Kind)).Contains(L.Want);
				if (C[i].Kind >= NumBirds && C[i].Kind < TreeCrab && !bAsked) continue;
				double Sc = 0, Wsum = 0;
				FVector Mid = FVector::ZeroVector;
				for (const FCand& O : C)
					if (FVector::DistSquared2D(FVector(O.P.X, O.P.Z, 0), FVector(C[i].P.X, C[i].P.Z, 0)) < 13.0 * 13.0) { Sc += O.W; if (O.Kind < NumBirds || O.Kind >= TreeCrab || (bAsked && O.Kind == C[i].Kind)) { Mid += O.P * O.W; Wsum += O.W; } }
				if (L.Rook < 0 && L.bOn && FVector::Dist2D(FVector(C[i].P.X, C[i].P.Z, 0), FVector(L.Tgt.X, L.Tgt.Z, 0)) < 8) Sc *= 1.6;   // (it stays on what it has)
				if (Sc > Best && Wsum > 0) { Best = Sc; BestP = Mid / Wsum; BestRook = -1; BestAcross = 26; BestWhat = KindName(C[i].Kind); }
			}
			if (Best < .05) return;
			L.bHave = true;
			L.Score = Best; L.Tgt = BestP; L.Rook = BestRook; L.Across = BestAcross; L.What = BestWhat;
		}

		bool Dry(double X, double Z) { return KeysGeo::LandH(X, Z) > -.05 || KeysGeoReal::MangroveAt(X, Z) > 0 || KeysDocks::InFootprint(X, Z, 1.5); }

		// Where the lens stands for what it is on: the far side of the boat from them and round one shoulder, so that
		// her middle is just outside the frame and the line of sight passes clear of her whole hull (a 43-footer lying
		// broadside is 13 m of boat 15 m from the lens); over open water, never among trees, on ground or on a dock.
		// With no such place (a creek, a pond) it stays astern and above, as it was.
		void Place(FLookIn& L, const AActor* Ctx, double X, double Z, double H, double Base, double BaseDist)
		{
			double HalfL = 3.5, HalfB = 1.3;
			if (const AKeysBoatPawn* Pw = Ctx ? Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(Ctx, 0)) : nullptr) { HalfL = Pw->Spec().Len * .5; HalfB = Pw->Spec().Beam * .5; }
			const FVector2D Boat(X, Z), T2(L.Tgt.X, L.Tgt.Z), Fwd(-FMath::Sin(H), -FMath::Cos(H));
			const double D = FMath::Max(4.0, FVector2D::Distance(T2, Boat));
			const FVector2D ToT = (T2 - Boat) / D;
			L.SideDist = FMath::Clamp(BaseDist * .5, 9.0, 16.0);
			// the lens: closes up on what is far, and takes in the whole of a key
			const double Fit = FMath::RadiansToDegrees(2 * FMath::Atan(L.Across * .5 / (D + L.SideDist * .8)));
			const double Fov = FMath::Clamp(L.Rook >= 0 ? Fit : FMath::Lerp(46.0, 40.0, FMath::Clamp((D - 25) / 55, 0.0, 1.0)), 34.0, 56.0);
			const double Off = FMath::Clamp((FMath::DegreesToRadians(Fov * .5) + .04) * (L.SideDist + D) / D, .3, 1.15), Opp = FMath::Atan2(-ToT.X, -ToT.Y);
			auto Clear = [&](double Aw)
			{
				const FVector2D C(X + FMath::Sin(Aw) * L.SideDist, Z + FMath::Cos(Aw) * L.SideDist), M = FMath::Lerp(Boat, C, .6);
				if (Dry(C.X, C.Y) || Dry(M.X, M.Y)) return false;
				for (int32 k = -3; k <= 3; k++)
				{
					const FVector2D Hp = Boat + Fwd * (HalfL * k / 3.0);
					if (FMath::PointDistToSegment(FVector(Hp.X, Hp.Y, 0), FVector(C.X, C.Y, 0), FVector(T2.X, T2.Y, 0)) < HalfB + .9) return false;
				}
				return true;
			};
			L.bBehind = true;
			for (int32 k = 0; k < 12 && L.bBehind; k++)
			{
				const double Sg = k % 2 ? -L.Sign : L.Sign, Aw = Opp + Sg * FMath::Min(Off + (k / 2) * .2, 1.5);
				if (!Clear(Aw)) continue;
				L.bBehind = false; L.Aw = Aw; L.Sign = Sg;
			}
			L.Fov = Fov + (L.bBehind ? 8 : 0);
			L.Look = L.bBehind ? FVector(X + ToT.X * D * .7, FMath::Lerp(1.8, L.Tgt.Y, .7), Z + ToT.Y * D * .7) : L.Tgt;
			(void)Base;
		}
	}

	FCloser CloserLook(double X, double Z, double H, double Speed, bool bBusy, double CamYaw, double CamPitchOff, double CamZoom, double BaseDist, double BaseHt, double Dt)
	{
		FCloser Out;
		FLookIn& L = GL;
		const bool bStale = GFrameCounter > L.Frame + 2;
		L.Frame = GFrameCounter;
		if (bStale) { L.K = 0; L.bOn = false; L.SlowT = 0; L.bHaveLast = false; L.Fov = 88; L.QuietT = 0; }
		AKeysMangroveEco* Act = AKeysMangroveEco::Get();
		FEco* E = Act ? Act->State() : nullptr;
		if (!E || L.bOff || BirdsBefore()) { L.K = 0; L.bOn = false; L.Fov = 88; return Out; }
		const double Sp = FMath::Abs(Speed), Base = H + CamYaw;
		const FVector2D Boat(X, Z);
		// ---- the player's own hand on the camera (a drag, the wheel, C; cam= in a script): it is his until she has
		// been under way again. (Above 5 m/s the pawn itself swings the camera back astern, after this is asked: that is
		// not his hand, nor is the change seen on the first frame she is below 5 m/s again.)
		// (nor is a change while a menu is up, a fish is on or the run is starting: those set the camera themselves)
		L.UserT += Dt;
		L.QuietT = bBusy ? 0 : L.QuietT + Dt;
		if (L.bHaveLast && L.QuietT > .6 && Sp <= 5 && L.LastSp <= 5 && (FMath::Abs(CamYaw - L.LastYaw) > 1e-9 || FMath::Abs(CamPitchOff - L.LastPitch) > 1e-9 || FMath::Abs(CamZoom - L.LastZoom) > 1e-9))
		{
			L.UserT = 0;
			if (!L.bHeld && (L.bOn || L.K > 0 || Sp < ReleaseMs)) { L.bHeld = true; if (L.bOn || L.K > .01) UE_LOG(LogTemp, Log, TEXT("KREco closer look: the player moved the camera (yaw %.2f, pitch %.2f, zoom %.2f): it is his until she has been under way again"), CamYaw, CamPitchOff, CamZoom); }
		}
		L.LastYaw = CamYaw; L.LastPitch = CamPitchOff; L.LastZoom = CamZoom; L.LastSp = Sp; L.bHaveLast = true;
		if (L.bHeld && Sp > ReleaseMs && L.UserT > 3) L.bHeld = false;
		L.SlowT = Sp < EngageMs ? L.SlowT + Dt : 0;
		// ---- what there is to look at, asked about twice a second while she is slow
		const bool bCan = !bBusy && !L.bHeld;
		if (bCan && Sp < ReleaseMs)
		{
			L.PickT -= Dt;
			if (E->Showers.Num() && E->Showers.Last().Id != L.Shower) { L.Shower = E->Showers.Last().Id; L.PickT = 0; }
			if (L.PickT <= 0) { L.PickT = L.bOn ? .8 : .4; Pick(*E, Boat, L); if (L.bHave) Place(L, Act, X, Z, H, Base, BaseDist); }
		}
		const bool bWant = bCan && L.bHave && (L.bOn ? Sp < ReleaseMs : L.SlowT > 1.2);
		if (bWant != L.bOn)
		{
			L.bOn = bWant;
			if (bWant)
			{
				L.Ons++; L.OnT = 0;
				L.A = Base;
				// which shoulder to look over: the one the camera is nearer already
				const FVector2D ToT = (FVector2D(L.Tgt.X, L.Tgt.Z) - Boat).GetSafeNormal();
				const double Opp = FMath::Atan2(-ToT.X, -ToT.Y);
				L.Sign = WrapPi(Base - Opp) >= 0 ? 1 : -1;
				Place(L, Act, X, Z, H, Base, BaseDist);
				UE_LOG(LogTemp, Log, TEXT("KREco closer look: engaged at %.0f, %.0f, the boat making %.1f kn: on %s %.0f m off"), X, Z, Sp * 1.944, *L.What, FVector2D::Distance(FVector2D(L.Tgt.X, L.Tgt.Z), Boat));
			}
			else
			{
				L.Offs++;
				UE_LOG(LogTemp, Log, TEXT("KREco closer look: released at %.0f, %.0f after %.1f s: %s"), X, Z, L.OnT,
					L.bHeld ? TEXT("the player moved the camera") : bBusy ? TEXT("a line is out, a fish is on or the game is not running") : Sp >= ReleaseMs ? *FString::Printf(TEXT("she is making %.1f kn"), Sp * 1.944) : TEXT("nothing to look at here any more"));
			}
		}
		L.OnT += Dt;
		L.K = FMath::FInterpConstantTo(L.K, L.bOn ? 1.0 : 0.0, Dt, L.bOn ? .55 : 1.4);
		if (L.K <= 0) { L.Fov = 88; return Out; }   // (the plain chase view, untouched)
		const double Ks = L.K * L.K * (3 - 2 * L.K);
		// ---- the lens eases to where it was last placed (Place: asked again with each look round)
		if (L.bOn) L.A += WrapPi((L.bBehind ? Base : L.Aw) - L.A) * FMath::Min(1.0, Dt * 1.6);
		const double WantDist = L.bBehind ? BaseDist * .7 : L.SideDist, WantHt = L.bBehind ? BaseHt * .85 : FMath::Clamp(BaseHt * .4, 2.4, 3.6);
		Out.K = Ks;
		Out.Dist = FMath::Lerp(1.0, WantDist / FMath::Max(1.0, BaseDist), Ks);
		Out.Ht = FMath::Lerp(1.0, WantHt / FMath::Max(.5, BaseHt), Ks);
		Out.Yaw = WrapPi(L.A - Base) * Ks;
		Out.Look = L.Look;
		Out.Fov = float(FMath::Lerp(88.0, L.Fov, Ks));
		return Out;
	}

	float CloserFov()
	{
		const FLookIn& L = GL;
		if (GFrameCounter > L.Frame + 1 || L.K <= 0) return 88.f;
		const double Ks = L.K * L.K * (3 - 2 * L.K);
		return float(FMath::Lerp(88.0, L.Fov, Ks));
	}

	void CloserCmd(const FString& Arg)
	{
		FLookIn& L = GL;
		if (Arg == TEXT("off")) { L.bOff = true; UE_LOG(LogTemp, Log, TEXT("KREco closer look: switched off for this run (test)")); }
		else if (Arg == TEXT("on")) { L.bOff = false; UE_LOG(LogTemp, Log, TEXT("KREco closer look: switched on")); }
		else if (Arg.StartsWith(TEXT("want"))) { L.Want = Arg.Mid(5).TrimStartAndEnd(); L.PickT = 0; UE_LOG(LogTemp, Log, TEXT("KREco closer look: told to prefer '%s' (test)"), *L.Want); }
		else UE_LOG(LogTemp, Log, TEXT("KREco closer look: %s, in %.2f, lens %.0f deg, on %s; engaged %d times, released %d; %s%s"), L.bOn ? TEXT("engaged") : TEXT("not engaged"), L.K, CloserFov(),
			L.bHave ? *L.What : TEXT("nothing"), L.Ons, L.Offs, L.bHeld ? TEXT("held off: the player moved the camera") : TEXT("not held off"), L.bBehind ? TEXT("; the lens stays astern (no room beside her)") : TEXT(""));
	}

	// ---------------------------------------------------------------- the measure
	// How tall each bird is on the screen from the player's own view: every bird's own triangles are put through the
	// lens (no test camera), and the nearest one of each kind that is in the frame is told. Then the table: each kind
	// in its usual pose, 25 m and 60 m beyond the boat as this camera sees them.
	void PxReport(FEco& E, UWorld* World, const FString& Tag)
	{
		APlayerCameraManager* Cm = UGameplayStatics::GetPlayerCameraManager(World, 0);
		if (!Cm) return;
		FVector2D Vp(1600, 900);
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Vp);
		const FVector CL = Cm->GetCameraLocation();
		const FRotator CR = Cm->GetCameraRotation();
		const FVector Fw = CR.Vector(), Rt = FRotationMatrix(CR).GetUnitAxis(EAxis::Y), Up = FRotationMatrix(CR).GetUnitAxis(EAxis::Z);
		const double Fov = Cm->GetFOVAngle(), F = Vp.X * .5 / FMath::Tan(FMath::DegreesToRadians(Fov * .5));
		const FVector BoatU = KeysGeo::ToUE(E.Boat.X, 0, E.Boat.Y);
		const double CamBoat = FVector::Dist2D(CL, BoatU) / 100;
		UE_LOG(LogTemp, Log, TEXT("KREco px%s: the player's view at %.0f x %.0f, lens %.1f deg (%.0f px a radian), %.1f m from the boat and %.1f m above the water; closer look %s"),
			*Tag, Vp.X, Vp.Y, Fov, F, CamBoat, CL.Z / 100 - E.Sea, GL.bOn ? TEXT("engaged") : TEXT("not engaged"));
		struct FSeen { double DBoat = 1e9, DLens = 0, Tall = 0, Wide = 0, M = 0, X = 0, Y = 0; int32 N = 0; uint8 State = 0; };
		FSeen Seen[NumBirds];
		FSoup Tmp;
		for (TPair<int64, FTileE*>& T : E.Tiles)
			for (const FBird& B : T.Value->Birds)
			{
				if (B.State == BAway) continue;
				const FVector Pu = KeysGeo::ToUE(B.P.X, B.P.Y, B.P.Z);
				const double Dl = FVector::Dist(Pu, CL) / 100;
				if (Dl > 400) continue;
				Tmp.Reset();
				BirdSoup(Tmp, B, false, 0);
				double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9, Z0 = 1e9, Z1 = -1e9;
				bool bFront = true;
				for (const FVector& V : Tmp.V)
				{
					const FVector Rl = V - CL;
					const double Dz = FVector::DotProduct(Rl, Fw);
					if (Dz < 50) { bFront = false; break; }
					const double Sx = Vp.X * .5 + FVector::DotProduct(Rl, Rt) / Dz * F, Sy = Vp.Y * .5 - FVector::DotProduct(Rl, Up) / Dz * F;
					X0 = FMath::Min(X0, Sx); X1 = FMath::Max(X1, Sx); Y0 = FMath::Min(Y0, Sy); Y1 = FMath::Max(Y1, Sy);
					Z0 = FMath::Min(Z0, V.Z); Z1 = FMath::Max(Z1, V.Z);
				}
				if (!bFront || X1 < 0 || X0 > Vp.X || Y1 < 0 || Y0 > Vp.Y) continue;
				FSeen& S = Seen[B.Kind];
				S.N++;
				const double Db = FVector2D::Distance(FVector2D(B.P.X, B.P.Z), E.Boat);
				if (Db < S.DBoat) { S.DBoat = Db; S.DLens = Dl; S.Tall = Y1 - Y0; S.Wide = X1 - X0; S.M = (Z1 - Z0) / 100; S.X = (X0 + X1) * .5; S.Y = (Y0 + Y1) * .5; S.State = B.State; }
			}
		static const TCHAR* StateName[] = {TEXT("standing"), TEXT("walking"), TEXT("striking"), TEXT("flying"), TEXT("perched"), TEXT("drying its wings"), TEXT("roosting"), TEXT("hovering"), TEXT("diving"), TEXT("swimming"), TEXT("away"), TEXT("on the wing")};
		for (int32 k = 0; k < NumBirds; k++)
			if (Seen[k].N) UE_LOG(LogTemp, Log, TEXT("KREco px%s in frame: %s (%s): %.0f px tall, %.0f px across (%.2f m tall), %.0f m from the boat, %.0f m from the lens, at pixel %.0f, %.0f; %d of the kind in frame"),
				*Tag, KindName(k), StateName[Seen[k].State], Seen[k].Tall, Seen[k].Wide, Seen[k].M, Seen[k].DBoat, Seen[k].DLens, Seen[k].X, Seen[k].Y, Seen[k].N);
		// the table: its usual pose, straight beyond the boat
		const FVector2D Cam2(CL.X / 100, CL.Y / 100);
		FVector2D Ahead = (E.Boat - Cam2).GetSafeNormal();
		if (Ahead.IsNearlyZero()) Ahead = FVector2D(1, 0);
		for (int32 k = 0; k < NumBirds; k++)
		{
			FBird B;
			PoseBird(B, k);
			Tmp.Reset();
			BirdSoup(Tmp, B, false, 0);
			// (a wader's feet are drawn down into the mud: its height is what stands above its feet)
			double Z0 = 1e9, Z1 = -1e9, R = 0;
			for (const FVector& V : Tmp.V) { Z0 = FMath::Min(Z0, FMath::Max(V.Z, 0.0)); Z1 = FMath::Max(Z1, V.Z); R = FMath::Max(R, FVector::Dist2D(V, FVector::ZeroVector)); }
			const bool bWing = k == Tern || k == Osprey;
			const double M = bWing ? R / 100 * 2 : (Z1 - Z0) / 100;
			double Px[2];
			for (int32 i = 0; i < 2; i++)
			{
				const FVector2D P = E.Boat + Ahead * (i ? 60.0 : 25.0);
				Px[i] = M * F / FMath::Max(1.0, FVector::Dist(KeysGeo::ToUE(P.X, E.Sea + 1, P.Y), CL) / 100);
			}
			UE_LOG(LogTemp, Log, TEXT("KREco px%s table: %s, model scale %.2f, %.2f m %s: %.0f px at 25 m from the boat, %.0f px at 60 m"), *Tag, KindName(k), BirdScale(k), M, bWing ? TEXT("across the wings (it is seen on the wing)") : TEXT("tall"), Px[0], Px[1]);
		}
	}

	// is the player's boat afloat where she lies? (the pawn's own rule: depth at this tide against what the hull draws)
	void AfloatReport(FEco& E, UWorld* World, const FString& Tag)
	{
		const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(World, 0));
		if (!P) return;
		const double D = KeysGeo::DepthFast(P->BoatX(), P->BoatZ()) + KeysGeo::TideRise(), Hull = P->TrimProfile().HullDraftM;
		UE_LOG(LogTemp, Log, TEXT("KREco afloat %s: the boat at %.0f, %.0f, heading %.3f, making %.1f kn, lies in %.2f m at this tide (charted %.2f m, tide %+.2f m); her hull grounds at %.2f m: %s"),
			*Tag, P->BoatX(), P->BoatZ(), P->BoatH(), FMath::Abs(P->GetSpeed()) * 1.944, D, KeysGeo::DepthAt(P->BoatX(), P->BoatZ()), KeysGeo::TideRise(), Hull, D < Hull ? TEXT("AGROUND") : TEXT("afloat"));
	}
}
