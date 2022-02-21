#pragma once

#include "Math.h"

class Camera
{
public:
	Camera()
	{
	};

	Camera(Quaternion Quat, Vector3f Pos, float Fov) : 
		Orientation(Quat), 
		Position(Pos),
		FOV(Fov) 
	{
	};

	Vector3f GetUpVector();
	Vector3f GetForward();
	Vector3f GetRight();

	Quaternion Orientation;
	Vector3f Position;
	Vector3f BaseRight = Vector3f(1.0f, 0.0f, 0.0f);
	Vector3f BaseUp = Vector3f(0.0f, 1.0f, 0.0f);
	Vector3f BaseForward = Vector3f(0.0f, 0.0f, 1.0f);
	float FOV = 75.0f;
};