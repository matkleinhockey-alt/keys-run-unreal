#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysLife.h"
#include "KeysData.h"

namespace KeysKit
{
	FPose RidePose(double X, double Z, double H, double Len, double Beam, double T, double RollK)
	{
		using namespace KeysGeo;
		const double Amp = AmpAt(X, Z), FX = -FMath::Sin(H), FZ = -FMath::Cos(H), HL = Len * .45, RX = FMath::Cos(H), RZ = -FMath::Sin(H), HB = Beam * .5;
		const double Hb = WaveH(X + FX * HL, Z + FZ * HL, T, Amp), Hs = WaveH(X - FX * HL, Z - FZ * HL, T, Amp);
		const double Hp = WaveH(X - RX * HB, Z - RZ * HB, T, Amp), Hst = WaveH(X + RX * HB, Z + RZ * HB, T, Amp);
		return {(Hb + Hs) / 2, FMath::Atan2(Hb - Hs, Len), FMath::Atan2(Hst - Hp, FMath::Max(.5, Beam)) * RollK};
	}

	bool AppendFish(FKeysMesh& Dst, const FString& Key, double LenM, const FTransform& XfM)
	{
		static TMap<FString, FKeysMesh> Cache;
		FKeysMesh* F = Cache.Find(Key);
		if (!F)
		{
			FKeysMesh Raw;
			if (!AKeysLife::BuildCreatureMesh(Key, Raw, 1.0))
			{
				const KeysData::FSpecies* Sp = KeysData::Species(Key);
				KeysMesh::BuildFish(Raw, 1.0, Sp ? Sp->Color : FLinearColor(.6f, .65f, .7f), Key == TEXT("marlin") || Key == TEXT("sailfish") || Key == TEXT("swordfish"));
			}
			// normalise to one metre nose to tail, centred
			double X0 = 1e18, X1 = -1e18;
			for (const FVector& V : Raw.V) { X0 = FMath::Min(X0, V.X); X1 = FMath::Max(X1, V.X); }
			const double K = X1 > X0 ? 100.0 / (X1 - X0) : 1, Mid = (X0 + X1) / 2;
			for (FVector& V : Raw.V) V = FVector(V.X - Mid, V.Y, V.Z) * K;
			F = &Cache.Add(Key, MoveTemp(Raw));
		}
		const FTransform Xf(XfM.GetRotation(), XfM.GetLocation() * 100, FVector(LenM));
		const int32 Base = Dst.V.Num();
		for (int32 i = 0; i < F->V.Num(); i++)
		{
			Dst.V.Add(Xf.TransformPosition(F->V[i]));
			Dst.N.Add(Xf.TransformVectorNoScale(F->N[i]));
			Dst.UV.Add(FVector2D::ZeroVector);
			Dst.C.Add(F->C.IsValidIndex(i) ? F->C[i] : FLinearColor::White);
			if (Dst.K.Num() == Dst.V.Num() - 1) Dst.K.Add(F->K.IsValidIndex(i) ? F->K[i] : FVector2D::ZeroVector);
		}
		for (int32 Ix : F->T) Dst.T.Add(Base + Ix);
		return true;
	}

	FRotator PoseRot(const FPose& P, double H)
	{
		return FRotator(FMath::RadiansToDegrees(P.Pitch), YawDeg(H), FMath::RadiansToDegrees(P.Roll));
	}
}

namespace KeysTags
{
	static TArray<FKeysTag> GTags;
	void Add(const FVector& Game, const FString& Text, double MaxDist, bool bBubble)
	{
		if (GTags.Num() < 64) GTags.Add({Game, Text, MaxDist, bBubble});
	}
	TArray<FKeysTag>& Live() { return GTags; }
}
