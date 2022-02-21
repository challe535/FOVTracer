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

	friend Vector2<Type> operator+(Vector2<Type> v1, Vector2<Type> v2);
	friend Vector2<Type> operator-(Vector2<Type> v1, Vector2<Type> v2);
	friend Vector2<Type> operator-(Vector2<Type> v);
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

template<class Type>
Vector2<Type> operator+(Vector2<Type> v1, Vector2<Type> v2)
{
	v1.X += v2.X;
	v1.Y += v2.Y;
	v1.Z += v2.Z;
	return v1;
}

template<class Type>
Vector2<Type> operator-(Vector2<Type> v1, Vector2<Type> v2)
{
	return v1 + (-v2);
}

template<class Type>
Vector2<Type> operator-(Vector2<Type> v)
{
	v.X = -v.X;
	v.Y = -v.Y;
	v.Z = -v.Z;
	return v;
}
