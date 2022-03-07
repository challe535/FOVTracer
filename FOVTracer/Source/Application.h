#pragma once

#include "Core.h"
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

	/**
	* Cleanup resources.
	*/
	void Cleanup();

	static Application& GetApplication() { static Application Instance; return Instance; };

public:
	Input* InputHandler;
	float ElapsedTimeS = 0.f;

	float ViewportWidth;
	float ViewportHeight;
private:
	HWND Window = nullptr;
	ImGuiContext* UIContext = nullptr;
	Tracer RayTracer;
	Scene RayScene;

	float WindowWidth;
	float WindowHeight;
};

