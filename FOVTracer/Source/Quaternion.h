#pragma once

#include "DXMathUtil.h"

using namespace DXMath;

class Quaternion {
public:
	//default
	Quaternion() : s(1.0f), v(Vector3f(0, 0, 0)), euler(to_euler()) {}

	//direct specification
	Quaternion(float s, float x, float y, float z){
		this->s = s;
		v = Vector3f(x, y, z);
		euler = to_euler();
	}

	//from angle axis
	Quaternion(float a, Vector3f axis) {
		s = cosf(0.5f * a);
		v = axis * sinf(0.5f * a);
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
		v.X = sr * cp * cy - cr * sp * sy;
		v.Y = cr * sp * cy + sr * cp * sy;
		v.Z = cr * cp * sy - sr * sp * cy;

		euler = Vector3f(rx, ry, rz);
	}

	//from euler angle vector: pitch, yaw, roll order
	//Quaternion(DirectX::XMFLOAT3 eulers) : Quaternion(eulers.X, eulers.Y, eulers.Z) {} 
	//Was causing issues when multiplying with float3 since automatic conversion would happen. Removed for now.

	Vector3f eulers() { 
		Vector3f eulers;

		float rad2deg = 360.0f / DirectX::XM_2PI;
		eulers = Vector3f(fmodf(euler.X * rad2deg, 360.0f), fmodf(euler.Y * rad2deg, 360.0f), fmodf(euler.Z * rad2deg, 360.0f));

		return eulers;
	}

	friend Quaternion operator*(Quaternion q1, Quaternion q2) {
		Quaternion q;
		q.s = q1.s * q2.s - q1.v.Dot(q2.v);
		q.v = q2.v * q1.s + q1.v * q2.s + q1.v.Cross(q2.v);
		q.euler = q.to_euler();

		return q;
	}

	friend Vector3f operator*(Quaternion& q, Vector3f v) {
		Vector3f t = (q.v * 2.0f).Cross(v);
		Vector3f p = v + t * q.s + q.v.Cross(t);

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
	Vector3f v;
	Vector3f euler;

	Vector3f to_euler() {
		Vector3f eulers;

		//attitude (roll)
		float sinp = 2 * (v.X * v.Y + v.Z * s);
		eulers.Z = asinf(sinp);

		//heading (yaw)
		float sinr_cosp = 2 * (s * v.Y - v.X * v.Z);
		float cosr_cosp = 1 - 2 * (v.Y * v.Y - v.X * v.X);
		eulers.Y = atan2f(sinr_cosp, cosr_cosp);
		
		//bank (roll)
		float siny_cosp = 2 * (s * v.X - v.Z * v.Y);
		float cosy_cosp = 1 - 2 * (v.X * v.X - v.Z * v.Z);
		eulers.X = atan2f(siny_cosp, cosy_cosp);


		float rad2deg = 360.0f / DirectX::XM_2PI;
		eulers = Vector3f(fmodf(eulers.X * rad2deg, 360.0f), fmodf(eulers.Y * rad2deg, 360.0f), fmodf(eulers.Z * rad2deg, 360.0f));

		return eulers;
	}
};