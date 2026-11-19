// How the hull moves on the water: heave, pitch and roll from buoyancy points sampled on the same wave field the
// water is drawn with (KeysGeo::WaveH, plus every boat's wake), so the boat rides exactly the waves you see.
// Numbers per hull live in the boat's profile (KeysTrim.h, DA_TrimTuning, category Motion).
//
//  - 8-12 points (bow, mid and stern on both sides, and the keel): each pushes up by how deep it is under the surface.
//    The sum heaves her, the fore-aft imbalance pitches her, the side-to-side imbalance rolls her. Pitch is damped
//    harder than roll, so she settles fore and aft but keeps rolling in a beam sea.
//  - mass: the draft is the mass over the waterplane, and the heave frequency follows from it (sqrt(g / draft)), so a
//    light skiff sits high and bounces on every ripple while a heavy sportfisher sits deep, moves slowly, and spans
//    (averages out) the short chop.
//  - planing: a hump first (the stern squats, the bow climbs), then the hull lifts out as she gets up on plane.
//  - head seas: the bow rises to each face; a face coming up at it fast enough slams (camera shake, spray: Slam).
//    Following seas: the back of a wave pushes you along (surfing), running into the back of the next one buries the
//    bow (stuffing: speed lost, spray), and the stern gets slewed round. Beam seas: she rolls.
//  - weight aboard: the people where they actually stand and the catch in the fish box trim and list her.
//  - trim (UpdateTrim): its pitch and roll targets ride on top through TrimPitch / TrimRoll.
// Sub-stepped at 240 Hz whatever the frame rate, so it is the same at 30 fps and at 120.
#include "KeysBoatPawn.h"
#include "KeysTrim.h"
#include "KeysGeo.h"
#include "KeysData.h"
#include "KeysExtras.h"
#include "KeysLife.h"
#include "KeysAudio.h"

using namespace KeysGeo;
using namespace KeysData;

void AKeysBoatPawn::UpdateHullMotion(double Dt, double T, double F, double HullDraft)
{
	const FBoatSpec& S = Spec();
	const FKeysTrimProfile& P = TrimProfile();
	const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
	const double Amp = AmpAt(X, Z);

	// ---- the hull: draft from mass, the planing hump and lift
	const double Awp = FMath::Max(1.0, S.Len * S.Beam * P.WaterplaneCoef);
	const double Dk = FMath::Clamp(P.MassKg / (1025.0 * Awp), .1, 1.2);
	const double Hump = FMath::Exp(-FMath::Square((F - P.HumpSpeedFrac) / .13)) * FMath::SmoothStep(.1, .4, FMath::Abs(Thr));
	const double Planed = FMath::SmoothStep(.3, .62, F);
	const double Ride = Dk * (1 + P.HumpSink * Hump - P.PlaneLift * Planed * (.8 + .4 * TrimV));
	const double PlanePitch = FMath::DegreesToRadians(P.HumpBowDeg) * Hump + .035 * F + TrimPitch + TrimVent * .03 * FMath::Sin(T * 9);

	// ---- the points
	TArray<double, TInlineAllocator<6>> Along, Across;
	for (const float A : P.PointsAlong) Along.Add(A);
	for (const float A : P.PointsAcross) Across.Add(A);
	if (Along.Num() == 0) Along = {.44, .15, -.15, -.44};
	if (Across.Num() == 0) Across = {-.42, 0, .42};
	const double NPts = Along.Num() * Across.Num();
	const double K = 9.8 / (NPts * Dk);                                    // each point's lift per metre under, per unit mass
	const double Ip = FMath::Square(P.PitchGyradius * S.Len), Ir = FMath::Square(P.RollGyradius * S.Beam);
	double Sz2 = 0, Sx2 = 0;
	for (const double A : Along) Sz2 += Across.Num() * FMath::Square(A * S.Len);
	for (const double A : Across) Sx2 += Along.Num() * FMath::Square(A * S.Beam);
	const double WH = FMath::Sqrt(9.8 / Dk), WP = FMath::Sqrt(K * Sz2 / Ip), WR = FMath::Sqrt(K * Sx2 / FMath::Max(.01, Ir));
	const double MaxSub = Dk + P.FreeboardM;

	// ---- the weight aboard, as a heave, pitch and roll load per unit of the boat's mass
	double LoadKg = 0, LoadX = 0, LoadY = 0;
	{
		TArray<FVector, TInlineAllocator<16>> People;
		if (const AKeysExtras* E = AKeysExtras::Get(this)) E->AboardPositions(People);
		if (IsLineOut() || FState == EKeysFish::Caught) People.Add(FishSpotLocal());   // you, at the rail
		for (const FVector& Pp : People) { LoadKg += P.PersonKg; LoadX += P.PersonKg * Pp.X; LoadY += P.PersonKg * Pp.Y; }
		const double FishKg = CoolerLb() * .4536;
		LoadKg += FishKg; LoadX += FishKg * P.FishBoxAlong * S.Len; LoadY += FishKg * P.FishBoxAcross;
	}
	const double M = FMath::Max(500.0, double(P.MassKg));
	const double LoadHeave = -9.8 * LoadKg / M, LoadPitch = -9.8 * LoadX / M * P.LoadEffect, LoadRoll = -9.8 * LoadY / M * P.LoadEffect;
	LoadListDeg = FMath::RadiansToDegrees(LoadRoll / FMath::Max(.01, WR * WR * Ir));
	LoadTrimDeg = FMath::RadiansToDegrees(LoadPitch / FMath::Max(.01, WP * WP * Ip));

	// ---- every boat's wake near us (other boats' rock you; your own rings are too young to count under your hull)
	static TArray<FLiveWake> Wakes;
	GatherWakes(X, Z, S.Len + 30, T, Wakes);

	// ---- integrate at 240 Hz
	const int32 Sub = FMath::Clamp(FMath::CeilToInt(Dt * 240), 1, 24);
	const double HDt = Dt / Sub;
	int32 Wet = 0;
	double BowSub = 0, BowRise = 0;
	for (int32 It = 0; It < Sub; It++)
	{
		double Fy = 0, Tp = 0, Tr = 0;
		Wet = 0;
		const double Spn = FMath::Sin(Pitch), Srn = FMath::Sin(Roll), Tt = T - Dt + HDt * (It + 1);
		const bool bLast = It == Sub - 1;
		for (const double Al : Along)
			for (const double Ac : Across)
			{
				const double Px = Ac * S.Beam, Pa = Al * S.Len;   // across (+ starboard), along (+ bow)
				const double Wx = X + Fx * Pa + Rx * Px, Wz = Z + Fz * Pa + Rz * Px;
				const double Yb = Y + Pa * Spn + Px * Srn - Ride;
				const double Sea = WaveH(Wx, Wz, Tt, Amp, false) + P.WakeResponse * WakeHFrom(Wakes, Wx, Wz);
				const double Sbm = FMath::Clamp(Sea - Yb, 0.0, MaxSub);
				if (Sbm > 0) Wet++;
				if (Al > .3 && bLast)
				{
					// the bow: how deep it is and how fast the wave face is coming up at it
					BowSub = FMath::Max(BowSub, Sbm);
					const double Rise = (Sea - WaveH(Wx, Wz, Tt - .05, Amp, false)) / .05 - (VY + VP * Pa * FMath::Cos(Pitch));
					BowRise = FMath::Max(BowRise, Rise);
				}
				const double Fi = K * Sbm;
				Fy += Fi;
				Tp += Fi * Pa;
				Tr += Fi * Px;
			}
		const double InW = Wet > 0 ? 1 : .15, WetK = Wet ? 1 : 0;
		VY += (Fy - 9.8 + LoadHeave * WetK - 2 * P.HeaveDamping * WH * VY * InW) * HDt;
		Y += VY * HDt;
		VP += ((Tp + LoadPitch * WetK) / Ip + PlanePitch * WP * WP * WetK - (Wet ? 0 : .5) - 2 * P.PitchDamping * WP * VP * InW) * HDt;
		Pitch += VP * HDt;
		VR += ((Tr + LoadRoll * WetK) / FMath::Max(.01, Ir) + (Steer * F * (S.bCat ? .05 : .16) + TrimRoll) * WR * WR * WetK - 2 * P.RollDamping * WR * VR * InW) * HDt;
		Roll += VR * HDt;
	}
	Pitch = FMath::Clamp(Pitch, -.6, .75);
	Roll = FMath::Clamp(Roll, -.7, .7);

	// ---- landings and slams
	const bool bWasAir = bAir;
	bAir = Wet == 0;
	if (bWasAir && !bAir && VY < -2.2) Slam(VY);
	// head sea: running into a steep face, the bow buries and slams without ever leaving the water
	else if (!bAir && F > .35 && BowSub > Dk * .9 && BowRise > P.SlamRiseMs / FMath::Sqrt(TrimSlamK) && T - BowSlamT > .45)
	{
		BowSlamT = T;
		Slam(-(BowRise - P.SlamRiseMs * .6) * (.8 + .6 * F) * (S.bCat ? .75 : 1));
	}

	// ---- following seas: stuffing the bow into the back of the wave ahead, and the stern slewing round
	const FVector2D Sl = WaveSlope(X, Z, T, Amp);
	const double SeaAlong = Sl.X * Fx + Sl.Y * Fz, SeaBeam = Sl.X * Rx + Sl.Y * Rz;
	const double Wd = WindDir(), Following = FMath::Clamp(FMath::Cos(Wd) * Fx + FMath::Sin(Wd) * Fz, 0.0, 1.0);
	if (!bAir && F > .2 && Following > .3 && BowSub > Dk * P.StuffDepth && VP < 0)
	{
		Speed *= 1 - FMath::Min(.5, P.StuffDrag * Dt * (BowSub / (Dk * P.StuffDepth) - .6));
		VP -= .6 * Dt * (BowSub / Dk);
		if (T - StuffT > .8)
		{
			StuffT = T;
			Shake = FMath::Max(Shake, .35);
			if (AKeysLife* L = AKeysLife::Get(this))
				for (const double Sd : {-1.0, 1.0})
				{
					const double BX = X + Fx * S.Len * .4 + Rx * Sd * S.Beam * .45, BZ = Z + Fz * S.Len * .4 + Rz * Sd * S.Beam * .45;
					L->BowSpray(FVector(BX, WaveH(BX, BZ, T, AmpAt(BX, BZ)) + .1, BZ), FVector2D(Rx * Sd, Rz * Sd), FVector2D(Fx * Speed, Fz * Speed), 40, 1.1);
				}
			if (T - StuffMsgT > 20) { StuffMsgT = T; ShowToast(TEXT("Stuffed the bow into the back of a wave - ease off and trim up a touch in a following sea.")); }
		}
	}
	if (!bAir && F > .25 && Following > .3) H += SeaBeam * P.BroachYaw * Following * F * Dt;   // the stern gets slewed round
	// surfing: on the face of a wave coming up astern the bow points downhill and gravity drives her along; climbing the
	// back of the one ahead takes speed off again
	double Surf = 0;
	if (!bAir && Following > .3 && Speed > 1)
	{
		Surf = FMath::Clamp(-9.8 * SeaAlong * P.SurfPush * Following, -2.5, 3.5);
		Speed += Surf * Dt;
	}

	if (DepthFast(X, Z) < HullDraft && Y < -.15) { Y = -.15; VY = FMath::Max(0.0, VY); }   // aground: sitting on the bottom
	RideNow = Ride;
	DkNow = Dk;
	if (bScriptLog && (HullLogT -= Dt) <= 0)
	{
		HullLogT = .1;
		UE_LOG(LogTemp, Log, TEXT("KRHull t %.2f dt %.4f sub %d dk %.2f ride %.2f y %.3f pitch %.4f roll %.4f kn %.1f hump %.2f load %.0fkg list %.2f trim %.2f wet %d air %d follow %.2f surf %.2f"),
			T, Dt, Sub, Dk, Ride, Y, Pitch, Roll, KnotsNow(), Hump, LoadKg, LoadListDeg, LoadTrimDeg, Wet, bAir ? 1 : 0, Following, Surf);
	}
}
