#include "pch.h"
#include "Camera.h"



Vector3f Camera::GetUpVector()
{
	return (Orientation * BaseUp).Normalized();
}

Vector3f Camera::GetForward()
{
	return (Orientation * BaseForward).Normalized();
}

Vector3f Camera::GetRight()
{
	return (Orientation * BaseRight).Normalized();
}