#pragma once

template<class Type>
class Vector2
{
public:
	Type X;
	Type Y;

	Vector2();
	Vector2(Type x, Type y);

	static Vector2<Type> One();
	static Vector2<Type> Zero();
};

typedef Vector2<float> Vector2f;
typedef Vector2<double> Vector2d;

template<class Type>
Vector2<Type>::Vector2()
{
	X = 0;
	Y = 0;
}

template<class Type>
Vector2<Type>::Vector2(Type x, Type y)
{
	X = x;
	Y = y;
}

template<class Type>
Vector2<Type> Vector2<Type>::One()
{
	return Vector2<Type>(1, 1);
}

template<class Type>
Vector2<Type> Vector2<Type>::Zero()
{
	return Vector2<Type>();
}
