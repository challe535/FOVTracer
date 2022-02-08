#pragma once

#include "Math.h"


class Transform
{
public:
	void Translate(Vector3f Translation);
	void Rotate(Vector3f EulerRotations);
	void Rotate(float Angle, Vector3f Axis);
	void SetScale(float scale);
	void GetScale();

	void GetMatrix();

private:
	Vector3f Translation;
	Vector3f Rotation;
	Vector3f Scale;
};

