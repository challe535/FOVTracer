#include "pch.h"
#include "Math.h"

namespace Math
{
	float sign(float x)
	{
		float eps = 0.001;

		return x < -eps ? -1 : (x > eps ? 1 : 0);
	}
}