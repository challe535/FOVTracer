#include "pch.h"
#include "Input.h"
#include "Log.h"

Input::Input()
{
	PrimaryScreenSize = Vector2f(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

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

void Input::UpdateMouseInfo()
{
	POINT CursorPos;
	GetCursorPos(&CursorPos);

	MousePos = PrimaryScreenSize * 0.5f;
	MouseDelta = Vector2f(CursorPos.x - MousePos.X, CursorPos.y - MousePos.Y);

	SetCursorPos(MousePos.X, MousePos.Y);
}

uint8_t Input::IsKeyDown(KeyCode Code)
{
	return GetAsyncKeyState(Code) & 0x8000 ? 1u : 0u;
}