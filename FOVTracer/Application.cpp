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
	App.Run();

	return ExitCode;
}

void Application::Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title)
{
	HRESULT HR = AppWindow::Create(width, height, instance, Window, title);
	Utils::Validate(HR, L"Error: Failed to create window!");

	AllocConsole();

	Log::Init();

	Scene TestScene;
	SceneObject TestObject;
	TestObject.LoadFromPath(Utils::GetResourcePath("models/quad.obj"));
	TestScene.AddSceneObject(TestObject);
	
	LUM_CORE_INFO("{0} objects in scene.", TestScene.GetNumSceneObjects());

	//Initalize and configure tracer
}

void Application::Run()
{
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//App specific code goes here
		//Feed scene into tracer and tell it to trace the scene
	}

	Cleanup();
}

void Application::Cleanup()
{
	DestroyWindow(Window);
}