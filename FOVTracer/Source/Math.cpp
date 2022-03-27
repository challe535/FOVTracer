#include "pch.h"
#include "Math.h"

std::mt19937 gen(1299412u);

namespace Math
{
	float sign(float x)
	{
		float eps = 0.001;

		return x < -eps ? -1 : (x > eps ? 1 : 0);
	}

	float randRangeUniformF(float lower, float upper)
	{
		std::uniform_real_distribution<float> dis(lower, upper);
		return dis(gen);
	}

	float haltonF(uint64_t index, float base)
	{
		float fIndex = index;
		float bInv = 1.0f / base;

		float f = 1;
		float r = 0;

		while (fIndex > 0.0001f)
		{
			f *= bInv;
			r += f * fmodf(fIndex, base);
			fIndex = floorf(fIndex * bInv);
		}

		return r;
	}
}