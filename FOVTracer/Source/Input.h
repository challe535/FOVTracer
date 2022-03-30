#pragma once

#include "Math.h"
#include "imgui/imgui.h"

#include <map>;

typedef UINT_PTR KeyCode;

#define W_KEY 0x57
#define A_KEY 0x41
#define S_KEY 0x53
#define D_KEY 0x44
#define Q_KEY 0x51
#define E_KEY 0x45
#define Z_KEY 0x5A
#define X_KEY 0x58
#define P_KEY 0x50

class Input
{
public:
	Input(ImGuiIO& io) :
		PrimaryScreenSize(Vector2f(static_cast<float>(GetSystemMetrics(SM_CXSCREEN)), static_cast<float>(GetSystemMetrics(SM_CYSCREEN))))
	{
		IO = &io;
	};

	void OnFrameEnd();
	void UpdateMouseInfo();
	void SetCursorToCenter();
	void SetCursorVisiblity(bool ShouldShow);

	uint8_t IsKeyDown(KeyCode Code);
	uint8_t IsKeyJustPressed(KeyCode Code);

	Vector2f MouseDelta;
	Vector2f MousePos;
	bool LockMouseToCenter = false;
private:
	Vector2f PrimaryScreenSize;
	bool CursorVisible = true;

	std::map<int, bool> PrevKeyStates;

	ImGuiIO* IO;
};