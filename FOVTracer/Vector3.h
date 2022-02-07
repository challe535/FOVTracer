#pragma once

template<class Type>
class Vector3
{
public:
	Type X;
	Type Y;
	Type Z;

	Vector3();
	Vector3(Type x, Type y, Type z);

	static Vector3<Type> One();
	static Vector3<Type> Zero();
};

typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;

template<class Type>
Vector3<Type>::Vector3()
{
	X = 0;
	Y = 0;
	Z = 0;
}

template<class Type>
Vector3<Type>::Vector3(Type x, Type y, Type z)
{
	X = x;
	Y = y;
	Z = z;
}

template<class Type>
Vector3<Type> Vector3<Type>::One()
{
	return Vector3<Type>(1, 1, 1);
}

template<class Type>
Vector3<Type> Vector3<Type>::Zero()
{
	return Vector3<Type>();
}