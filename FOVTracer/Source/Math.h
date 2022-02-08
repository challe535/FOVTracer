#pragma once

#include "Vector3.h"
#include "Vector2.h"


namespace Math
{
	template<class T>
	T max(T x, T y)
	{
		return x < y ? y : x;
	}
}