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
	Vector3(const Vector3<Type>& v);

	Vector3<Type> Cross(Vector3<Type> v);
	Type Dot(Vector3<Type> v);
	Vector3<Type> Normalized();
	Type Length();

	static Vector3<Type> One();
	static Vector3<Type> Zero();

	//Vector3<Type>& operator=(Vector3<Type> other);
	Vector3<Type>& operator*(Type s);
	Vector3<Type>& operator-();
	Vector3<Type>& operator+(Vector3<Type> v);
	Vector3<Type>& operator-(Vector3<Type> v);
	
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
Vector3<Type>::Vector3(const Vector3<Type>& v)
{
	X = v.X;
	Y = v.Y;
	Z = v.Z;
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

template<class Type>
Vector3<Type> Vector3<Type>::Cross(Vector3<Type> v)
{
	Vector3f Product;
	Product.X = Y * v.Z - Z * v.Y;
	Product.Y = Z * v.X - X * v.Z;
	Product.Z = X * v.Y - Y * v.X;

	return Product;
}

template<class Type>
Type Vector3<Type>::Dot(Vector3<Type> v)
{
	return X * v.X + Y * v.Y + Z * v.Z;
}

template<class Type>
Vector3<Type> Vector3<Type>::Normalized()
{
	float InvLength = 1.0f / Length();
	Vector3f Result;
	Result.X = X * InvLength;
	Result.Y = Y * InvLength;
	Result.Z = Z * InvLength;

	return Result;
}

template<class Type>
Type Vector3<Type>::Length()
{
	return sqrt(X * X + Y * Y + Z * Z);
}

template<class Type>
Vector3<Type>& Vector3<Type>::operator*(Type s)
{
	X *= s;
	Y *= s;
	Z *= s;
	return *this;
}

template<class Type>
Vector3<Type> operator*(Type s, Vector3<Type> v)
{
	v.X *= s;
	v.Y *= s;
	v.Z *= s;

	return v;
}

template<class Type>
Vector3<Type>& Vector3<Type>::operator-()
{
	X = -X;
	Y = -Y;
	Z = -Z;
	return *this;
}

template<class Type>
Vector3<Type>& Vector3<Type>::operator+(Vector3<Type> v)
{
	X += v.X;
	Y += v.Y;
	Z += v.Z;
	return *this;
}

template<class Type>
Vector3<Type>& Vector3<Type>::operator-(Vector3<Type> v)
{
	return *this + (-v);
}
