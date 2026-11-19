#include "KeysTrim.h"
#include "UObject/StrongObjectPtr.h"

double FKeysTrimProfile::IdealAt(double Fr) const
{
	if (IdealCurve.Num() == 0) return 0;
	if (Fr <= IdealCurve[0].X) return IdealCurve[0].Y;
	for (int32 I = 1; I < IdealCurve.Num(); I++)
		if (Fr <= IdealCurve[I].X)
		{
			const FVector2D A = IdealCurve[I - 1], B = IdealCurve[I];
			const double K = B.X > A.X ? (Fr - A.X) / (B.X - A.X) : 1;
			return FMath::Lerp(A.Y, B.Y, FMath::SmoothStep(0.0, 1.0, K));
		}
	return IdealCurve.Last().Y;
}

namespace
{
	FKeysTrimProfile Make(const TCHAR* Id, std::initializer_list<FVector2D> Curve, double Draft)
	{
		FKeysTrimProfile P;
		P.BoatId = FName(Id);
		P.IdealCurve = TArray<FVector2D>(Curve);
		P.PropDepthM = float(Draft);
		P.HullDraftM = float(Draft * .58);
		return P;
	}
}

UKeysTrimTuning::UKeysTrimTuning()
{
	Fallback = Make(TEXT("default"), {{0, -4}, {.3, -3}, {.5, 2}, {.75, 5}, {1, 7}}, .7);

	// the light single-engine bay boat: lively, gets on plane on its own, porpoises easily if you trim it out too far
	FKeysTrimProfile Robalo = Make(TEXT("robalo"), {{0, -4}, {.3, -2}, {.5, 2}, {.75, 5}, {1, 8}}, .6);
	Robalo.HoleShotDownGain = .25f; Robalo.PorpoiseStartDeg = 3.f; Robalo.PorpoiseAmpDeg = 4.f; Robalo.PorpoiseHz = 1.6f;
	Robalo.TabAuthorityDeg = 6.f; Robalo.CrewLeanDeg = 2.4f; Robalo.WindLeanDegPerMs = .3f; Robalo.PropStrikeCost = 450;
	Boats.Add(Robalo);

	FKeysTrimProfile Grady = Make(TEXT("grady"), {{0, -4}, {.3, -3}, {.5, 1.5}, {.75, 4.5}, {1, 6.5}}, .72);
	Grady.RoughTrimDownDeg = 6.f;   // a deep-V that wants to be driven in into a head sea
	Boats.Add(Grady);

	// catamarans: two hulls ride level on their own; tabs do less, porpoising is milder, but they blow out when trimmed high
	FKeysTrimProfile Freeman = Make(TEXT("freeman"), {{0, -3}, {.3, -1}, {.5, 2}, {.75, 4}, {1, 6}}, .82);
	Freeman.SweetBandDeg = 3.f; Freeman.PorpoiseAmpDeg = 2.f; Freeman.VentStartDeg = 6.f; Freeman.TabAuthorityDeg = 2.5f;
	Freeman.CrewLeanDeg = .8f; Freeman.WindLeanDegPerMs = .12f; Freeman.HoleShotUpLoss = .35f; Freeman.PropStrikeCost = 1600;
	Boats.Add(Freeman);

	FKeysTrimProfile Midnight = Make(TEXT("midnight"), {{0, -5}, {.3, -3}, {.5, 1}, {.75, 4}, {1, 6}}, .9);
	Midnight.HoleShotDownGain = .4f; Midnight.BowRiseDeg = 11.f; Midnight.RoughTrimDownDeg = 6.f; Midnight.PropStrikeCost = 2400;
	Boats.Add(Midnight);

	// the go-fast cat: tiny sweet spot high up the range, very loose at the top end
	FKeysTrimProfile Mti = Make(TEXT("mti"), {{0, -3}, {.3, -1}, {.5, 3}, {.75, 6.5}, {1, 9}}, 1.15);
	Mti.SweetBandDeg = 1.8f; Mti.PorpoiseStartDeg = 2.5f; Mti.PorpoiseAmpDeg = 2.5f; Mti.PorpoiseHz = 1.8f; Mti.VentStartDeg = 5.5f;
	Mti.TabAuthorityDeg = 2.5f; Mti.CrewLeanDeg = .7f; Mti.WindLeanDegPerMs = .1f; Mti.SlamPerDegUp = .09f; Mti.PropStrikeCost = 3200;
	Boats.Add(Mti);

	FKeysTrimProfile Yellowfin = Make(TEXT("yellowfin"), {{0, -4}, {.3, -3}, {.5, 1.5}, {.75, 4.5}, {1, 7}}, .76);
	Yellowfin.PropStrikeCost = 1200;
	Boats.Add(Yellowfin);

	FKeysTrimProfile Contender = Make(TEXT("contender"), {{0, -4}, {.3, -3}, {.5, 2}, {.75, 5}, {1, 7}}, .68);
	Contender.RoughTrimDownDeg = 6.f;
	Boats.Add(Contender);

	// the stepped go-fast monohull: wants a lot of trim at the top, punishes you hard for too much
	FKeysTrimProfile Nortech = Make(TEXT("nortech"), {{0, -4}, {.3, -2}, {.5, 3}, {.75, 7}, {1, 10}}, 1.05);
	Nortech.SweetBandDeg = 1.8f; Nortech.PorpoiseStartDeg = 2.5f; Nortech.PorpoiseAmpDeg = 4.5f; Nortech.PorpoiseHz = 1.7f;
	Nortech.VentStartDeg = 5.f; Nortech.BowSteerGain = 1.3f; Nortech.SlamPerDegUp = .09f; Nortech.PropStrikeCost = 2800;
	Boats.Add(Nortech);

	// inboard diesels on shafts: nothing to trim or tilt, the props sit under the hull; tabs only
	FKeysTrimProfile Viking = Make(TEXT("viking"), {{0, 0}, {1, 0}}, 1.45);
	Viking.bOutboards = false; Viking.TopSpeedBonus = 0; Viking.FuelBurnSaving = 0; Viking.HullDraftM = 1.2f;
	Viking.TabAuthorityDeg = 3.f; Viking.WindLeanDegPerMs = .18f; Viking.CrewLeanDeg = .5f; Viking.PropStrikeCost = 6500;
	Boats.Add(Viking);
}

const UKeysTrimTuning* UKeysTrimTuning::Get()
{
	static TStrongObjectPtr<UKeysTrimTuning> Asset;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Asset.Reset(LoadObject<UKeysTrimTuning>(nullptr, TEXT("/Game/KeysRun/Data/DA_TrimTuning.DA_TrimTuning")));
		UE_LOG(LogTemp, Log, TEXT("KRTrim tuning: %s"), Asset ? TEXT("DA_TrimTuning") : TEXT("defaults (no DA_TrimTuning asset)"));
	}
	return Asset ? Asset.Get() : GetDefault<UKeysTrimTuning>();
}

const FKeysTrimProfile& UKeysTrimTuning::For(const FString& BoatId)
{
	const UKeysTrimTuning* T = Get();
	const FName N(*BoatId);
	for (const FKeysTrimProfile& P : T->Boats) if (P.BoatId == N) return P;
	return T->Fallback;
}
