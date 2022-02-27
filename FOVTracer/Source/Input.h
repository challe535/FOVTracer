#pragma once

#include "Math.h"
#include "imgui/imgui.h"

typedef UINT_PTR KeyCode;

#define W_KEY 0x57
#define A_KEY 0x41
#define S_KEY 0x53
#define D_KEY 0x44
#define Q_KEY 0x51
#define E_KEY 0x45

class Input
{
public:
	Input(ImGuiIO& io) :
		PrimaryScreenSize(Vector2f(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN))) 
	{
		IO = &io;
	};

	void OnFrameEnd();
	void UpdateMouseInfo();
	void SetCursorToCenter();
	void SetCursorVisiblity(bool ShouldShow);

	uint8_t IsKeyDown(KeyCode Code);

	Vector2f MouseDelta;
	Vector2f MousePos;
	bool LockMouseToCenter = false;
private:
	Vector2f PrimaryScreenSize;
	bool CursorVisible = true;

	ImGuiIO* IO;
};