#include "KeysWeather.h"

namespace KeysWx
{
	// the rain columns under the storm cells: x, z, radius, strength
	static TArray<FVector4> GRain;

	double RainAt(double X, double Z)
	{
		double R = 0;
		for (const FVector4& C : GRain)
		{
			const double D2 = (FMath::Square(X - C.X) + FMath::Square(Z - C.Y)) / FMath::Square(FMath::Max(1.0, C.Z));
			if (D2 < 1) R = FMath::Max(R, C.W * (1 - D2) * (1 - D2));
		}
		return R;
	}
}
