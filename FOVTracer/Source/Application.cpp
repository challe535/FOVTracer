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

	Application& App = Application::GetApplication();
	App.Init(1280, 720, hInstance, L"FOVTracer");
	//App.Init(1920, 1080, hInstance, L"FOVTracer");
	App.Run();

	return ExitCode;
}

void Application::Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title)
{
	HRESULT HR = AppWindow::Create(width, height, instance, Window, title);
	Utils::Validate(HR, L"Error: Failed to create window!");

	SetCapture(Window);
	AllocConsole();

	Log::Init();

	RayScene.LoadFromPath(Utils::GetResourcePath("sponza/sponza.obj"), false);
	//RayScene.LoadFromPath(Utils::GetResourcePath("misc/bunny.obj"), true);
	CORE_INFO("{0} objects in scene.", RayScene.GetNumSceneObjects());

	RayScene.SceneCamera.Orientation *= Quaternion(0.0f, DirectX::XM_PI/2.0f, 0.0f);
	RayScene.SceneCamera.Position = Vector3f(0.0f, 175.0f, 0.0f);
	RayScene.SceneCamera.FOV = 75.0f;

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

		Camera& SceneCamera = RayScene.SceneCamera;

		//SceneCamera.Orientation *= Quaternion(InputHandler.MouseDelta.Y * DeltaTime, InputHandler.MouseDelta.X * DeltaTime, 0);

		float CameraForwardMove = InputHandler.IsKeyDown(W_KEY) - InputHandler.IsKeyDown(S_KEY);
		float CameraRightMove = InputHandler.IsKeyDown(D_KEY) - InputHandler.IsKeyDown(A_KEY);
		float CameraUpMove = InputHandler.IsKeyDown(E_KEY) - InputHandler.IsKeyDown(Q_KEY);

		float CameraSpeed = 100.0f;

		Vector3f CameraMove = CameraForwardMove * SceneCamera.GetForward() + CameraRightMove * SceneCamera.GetRight() + CameraUpMove * SceneCamera.GetUpVector();

		SceneCamera.Position = SceneCamera.Position + CameraMove * DeltaTime * CameraSpeed;

		RayTracer.Update(RayScene);
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

		InputHandler.OnFrameEnd();
	}

	Cleanup();
}

void Application::Cleanup()
{
	ReleaseCapture();

	RayScene.Clear();
	RayTracer.Cleanup();
	DestroyWindow(Window);
}