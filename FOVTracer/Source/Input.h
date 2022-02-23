#pragma once

#include "Math.h"

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
	Input();

	void OnFrameEnd();
	void OnMouseMove(int CurrentPosX, int CurrentPosY);
	void UpdateMouseInfo();

	uint8_t IsKeyDown(KeyCode Code);

	Vector2f MouseDelta;
	Vector2f MousePos;

private:
	Vector2f PrimaryScreenSize;
};