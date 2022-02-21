#include "pch.h"
#include "Input.h"

void Input::OnFrameEnd()
{
	MouseDelta = Vector2f(0,0);
}

void Input::OnMouseMove(int CurrentPosX, int CurrentPosY)
{
	auto NewPos = Vector2f(CurrentPosX, CurrentPosY);
	MouseDelta = Vector2f(NewPos.X - MousePos.X, NewPos.Y - MousePos.Y);
	MousePos = NewPos;
}

uint8_t Input::IsKeyDown(KeyCode Code)
{
	return GetKeyState(Code) & 0x8000 ? 1u : 0u;
}