#include "KeysKit.h"
#include "KeysGeo.h"

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
