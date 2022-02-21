#include "pch.h"
#include "DXMathUtil.h"

namespace DXMath
{
	float DXFloat3Dot(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	DirectX::XMFLOAT3 DXFloat3MulScalar(DirectX::XMFLOAT3 v, float scalar)
	{
		return DirectX::XMFLOAT3(v.x * scalar, v.y * scalar, v.z * scalar);
	}

	DirectX::XMFLOAT3 DXFloat3AddFloat3(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2)
	{
		return DirectX::XMFLOAT3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
	}

	DirectX::XMFLOAT3 DXFloat3Cross(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2)
	{
		return DirectX::XMFLOAT3(
			v1.y * v2.z - v1.z * v2.y,
			v1.z * v2.x - v1.x * v2.z,
			v1.x * v2.y - v1.y * v2.x
		);
	}

	DirectX::XMFLOAT3 Vector3fToDXFloat3(Vector3f v)
	{
		return DirectX::XMFLOAT3(v.X, v.Y, v.Z);
	}
}