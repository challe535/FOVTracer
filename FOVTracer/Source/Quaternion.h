#pragma once

#include "DXMathUtil.h"

using namespace DXMath;

class Quaternion {
public:
	//default
	Quaternion() : s(1.0f), v(DirectX::XMFLOAT3(0, 0, 0)), euler(to_euler()) {}

	//direct specification
	Quaternion(float s, float x, float y, float z){
		this->s = s;
		v = DirectX::XMFLOAT3(x, y, z);
		euler = to_euler();
	}

	//from angle axis
	Quaternion(float a, DirectX::XMFLOAT3 axis) {
		s = cos(0.5f * a);
		v = DXFloat3MulScalar(axis, sin(0.5f * a));
		euler = to_euler();
	}

	//from euler angles
	Quaternion(float rx, float ry, float rz) {
		float cy = (float)cos(rz * 0.5); //roll
		float sy = (float)sin(rz * 0.5); //roll
		float cp = (float)cos(ry * 0.5); //yaw
		float sp = (float)sin(ry * 0.5); //yaw
		float cr = (float)cos(rx * 0.5); //pitch
		float sr = (float)sin(rx * 0.5); //pitch

		s = cr * cp * cy + sr * sp * sy;
		v.x = sr * cp * cy - cr * sp * sy;
		v.y = cr * sp * cy + sr * cp * sy;
		v.z = cr * cp * sy - sr * sp * cy;

		euler = DirectX::XMFLOAT3(rx, ry, rz);
	}

	//from euler angle vector: pitch, yaw, roll order
	Quaternion(DirectX::XMFLOAT3 eulers) : Quaternion(eulers.x, eulers.y, eulers.z) {}

	DirectX::XMFLOAT3 eulers() { 
		DirectX::XMFLOAT3 eulers;

		float rad2deg = 360.0f / DirectX::XM_2PI;
		eulers = DirectX::XMFLOAT3(fmod(euler.x * rad2deg, 360.0f), fmod(euler.y * rad2deg, 360.0f), fmod(euler.z * rad2deg, 360.0f));

		return eulers;
	}

	friend Quaternion operator*(Quaternion q1, Quaternion q2) {
		Quaternion q;
		q.s = q1.s * q2.s - DXFloat3Dot(q1.v, q2.v);
		q.v = DXFloat3AddFloat3(DXFloat3AddFloat3(DXFloat3MulScalar(q2.v, q1.s), DXFloat3MulScalar(q1.v, q2.s)), DXFloat3Cross(q1.v, q2.v));
		q.euler = q.to_euler();

		return q;
	}

	friend DirectX::XMFLOAT3 operator*(Quaternion& q, DirectX::XMFLOAT3 v) {
		DirectX::XMFLOAT3 t = DXFloat3Cross(DXFloat3MulScalar(q.v, 2.0f), v);
		DirectX::XMFLOAT3 p = DXFloat3AddFloat3(DXFloat3AddFloat3(v, DXFloat3MulScalar(t, q.s)), DXFloat3Cross(q.v, t));

		return p;
	}

	Quaternion& operator=(Quaternion q) {
		s = q.s;
		v = q.v;
		euler = q.euler;

		return *this;
	}

	Quaternion& operator*=(Quaternion q) {
		 q = *this * q;
		 
		 return *this = q;
	}

private:
	float s = 0.0f;
	DirectX::XMFLOAT3 v;
	DirectX::XMFLOAT3 euler;

	DirectX::XMFLOAT3 to_euler() {
		DirectX::XMFLOAT3 eulers;

		//attitude (roll)
		float sinp = 2 * (v.x * v.y + v.z * s);
		eulers.z = asinf(sinp);

		//heading (yaw)
		float sinr_cosp = 2 * (s * v.y - v.x * v.z);
		float cosr_cosp = 1 - 2 * (v.y * v.y - v.x * v.x);
		eulers.y = atan2f(sinr_cosp, cosr_cosp);
		
		//bank (roll)
		float siny_cosp = 2 * (s * v.x - v.z * v.y);
		float cosy_cosp = 1 - 2 * (v.x * v.x - v.z * v.z);
		eulers.x = atan2f(siny_cosp, cosy_cosp);


		float rad2deg = 360.0f / DirectX::XM_2PI;
		eulers = DirectX::XMFLOAT3(fmod(eulers.x * rad2deg, 360.0f), fmod(eulers.y * rad2deg, 360.0f), fmod(eulers.z * rad2deg, 360.0f));

		return eulers;
	}
};