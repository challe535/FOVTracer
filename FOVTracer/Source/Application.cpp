#include "pch.h"
#include "Application.h"
#include "AppWindow.h"
#include "Log.h"

#include <stdlib.h>
#include <iostream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT ExitCode = EXIT_SUCCESS;

	Application App;
	App.Init(1280, 720, hInstance, L"FOVTracer");
	//App.Init(1920, 1080, hInstance, L"FOVTracer");
	App.Run();

	return ExitCode;
}

void Application::Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title)
{
	HRESULT HR = AppWindow::Create(width, height, instance, Window, title);
	Utils::Validate(HR, L"Error: Failed to create window!");

	AllocConsole();

	Log::Init();

	RayScene.LoadFromPath(Utils::GetResourcePath("sponza/sponza.obj"), false);
	//RayScene.LoadFromPath(Utils::GetResourcePath("misc/bunny.obj"), true);
	CORE_INFO("{0} objects in scene.", RayScene.GetNumSceneObjects());

	//Initalize and configure tracer
	TracerConfigInfo Config;
	Config.Height = height;
	Config.Width = width;
	Config.Instance = instance;
	Config.Vsync = true;

	RayTracer.Init(Config, Window, RayScene);
}

void Application::Run()
{
	MSG msg = { 0 };

	float DeltaTime = 0.0f;
	float FpsRunningAverage = 0.0f;
	float FpsTrueAverage = 0.0f;

	float TotalTimeSinceStart = 0.0f;
	uint64_t FrameCount = 0;

	while (WM_QUIT != msg.message)
	{
		auto const FrameStart = std::chrono::high_resolution_clock::now();

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//App specific code goes here
		//Feed scene into tracer and tell it to trace the scene
		RayTracer.Update();
		RayTracer.Render();

		auto const FrameEnd = std::chrono::high_resolution_clock::now();

		DeltaTime = std::chrono::duration<float>(FrameEnd - FrameStart).count();
		TotalTimeSinceStart += DeltaTime;
		FrameCount++;
		
		float FrameFpsAverage = 1.0f / DeltaTime;

		FpsRunningAverage = FpsRunningAverage * 0.9f + FrameFpsAverage * 0.1f;
		FpsTrueAverage = 1.0f * (static_cast<float>(FrameCount) / TotalTimeSinceStart);

		if (FrameCount % 200 == 0)
		{
			CORE_TRACE("FpsRunning = {0}, FpsTrue = {1}, FpsCurrent = {2}", FpsRunningAverage, FpsTrueAverage, FrameFpsAverage);
		}
	}

	Cleanup();
}

void Application::Cleanup()
{
	RayScene.Clear();
	RayTracer.Cleanup();
	DestroyWindow(Window);
}