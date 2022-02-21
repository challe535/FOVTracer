#pragma once

#include <DirectXMath.h>
#include "Vector3.h"

namespace DXMath
{
	float DXFloat3Dot(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2);

	DirectX::XMFLOAT3 DXFloat3MulScalar(DirectX::XMFLOAT3 v, float scalar);

	DirectX::XMFLOAT3 DXFloat3AddFloat3(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2);

	DirectX::XMFLOAT3 DXFloat3Cross(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2);

	DirectX::XMFLOAT3 Vector3fToDXFloat3(Vector3f v);
}