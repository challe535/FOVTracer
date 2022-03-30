#pragma once

#include "Core.h"
#include "AppWindow.h"
#include "Tracer.h"
#include "Input.h"
#include "imgui/imgui.h"

#define ImTextureID ImU64

class Application
{

public:
	Application() 
	{
	};
	Application(Application const&) = delete;
	void operator=(Application const&) = delete;

	/**
	* Initialize application resources.
	*/
	void Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title);

	/**
	* Run application loop. Returns exit code.
	*/
	void Run();

	void Resize(LONG newWidth, LONG newHeight);

	/**
	* Cleanup resources.
	*/
	void Cleanup();

	static Application& GetApplication() { static Application Instance; return Instance; };

public:
	Input* InputHandler = nullptr;
	float ElapsedTimeS = 0.f;
	uint64_t FrameCount = 0u;

	float ViewportWidth = 0.0f;
	float ViewportHeight = 0.0f;
private:
	HWND Window = nullptr;
	ImGuiContext* UIContext = nullptr;
	Tracer RayTracer;
	Scene RayScene;

	float WindowWidth = 0.0f;
	float WindowHeight = 0.0f;

	bool IsDLSSEnabled = false;
};

