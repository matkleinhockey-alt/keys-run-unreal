#pragma once

#include "CoreMinimal.h"

// Weather that the rest of the game reads (game meters, x east, z south).
namespace KeysWx
{
	// how hard it is raining on the water at (X, Z), 0..1. Thread-safe to read (the water mesh calls it in parallel).
	double RainAt(double X, double Z);
}
