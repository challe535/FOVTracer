#include "pch.h"
#include "Input.h"
#include "Log.h"

void Input::OnFrameEnd()
{
	MouseDelta = Vector2f(0,0);
}

void Input::UpdateMouseInfo()
{
	if (IO->WantCaptureMouse)
		return;

	POINT CursorPos;
	GetCursorPos(&CursorPos);

	MousePos = PrimaryScreenSize * 0.5f;
	MouseDelta = Vector2f(CursorPos.x - MousePos.X, CursorPos.y - MousePos.Y);

	if (LockMouseToCenter)
		SetCursorPos(MousePos.X, MousePos.Y);
}

uint8_t Input::IsKeyDown(KeyCode Code)
{
	if (IO->WantCaptureKeyboard)
		return 0u;

	return GetAsyncKeyState(Code) & 0x8000 ? 1u : 0u;
}

void Input::SetCursorToCenter()
{
	if (IO->WantCaptureMouse)
		return;

	Vector2f Center = PrimaryScreenSize * 0.5f;
	SetCursorPos(Center.X, Center.Y);
}

void Input::SetCursorVisiblity(bool ShouldShow)
{
	if (IO->WantCaptureMouse)
		return;

	if(ShouldShow && !CursorVisible)
		ShowCursor(CursorVisible = true);
	else if (!ShouldShow && CursorVisible)
		ShowCursor(CursorVisible = false);
}