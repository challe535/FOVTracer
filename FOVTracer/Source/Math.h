#pragma once

#include "DXMathUtil.h"
#include "Vector3.h"
#include "Vector2.h"
#include "Quaternion.h"


namespace Math
{
	template<class T>
	T max(T x, T y)
	{
		return x < y ? y : x;
	}

	float sign(float x);

}