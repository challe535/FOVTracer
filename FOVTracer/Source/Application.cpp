#include "pch.h"
#include "Application.h"
#include "AppWindow.h"
#include "Log.h"

#include "imgui/imgui_impl_win32.h"

#include <stdlib.h>
#include <iostream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT ExitCode = EXIT_SUCCESS;

	Application& App = Application::GetApplication();
	//App.Init(1280, 720, hInstance, L"FOVTracer");
	App.Init(1920, 1080, hInstance, L"FOVTracer");
	App.Run();

	return ExitCode;
}

void Application::Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title)
{
	HRESULT HR = AppWindow::Create(width, height, instance, Window, title);
	Utils::Validate(HR, L"Error: Failed to create window!");

	RECT OldClip, Clip;

	GetClipCursor(&OldClip);
	GetWindowRect(Window, &Clip);


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
	Config.Vsync = false;

	WindowHeight = ViewportHeight = height;
	WindowWidth = ViewportWidth = width;

	IMGUI_CHECKVERSION();
	UIContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(UIContext);

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Window);

	InputHandler = new Input(ImGui::GetIO());

	RayTracer.Init(Config, Window, RayScene);
}

void Application::Run()
{
	MSG msg = { 0 };

	TracerParameters TraceParams;
	ComputeParams ComputeParams;

	float DeltaTime = 0.0f;
	float FpsRunningAverage = 0.0f;
	float FpsTrueAverage = 0.0f;

	float ViewportRatio = 1.0f;

	uint64_t FrameCount = 0;
	bool LMouseClicked = false;

	while (WM_QUIT != msg.message)
	{
		auto const FrameStart = std::chrono::high_resolution_clock::now();

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (InputHandler->IsKeyDown(VK_ESCAPE))
			PostQuitMessage(0);

		InputHandler->UpdateMouseInfo();

		//Compute Params
		ComputeParams.fovealCenter = TraceParams.fovealCenter;
		ComputeParams.isFoveatedRenderingEnabled = TraceParams.isFoveatedRenderingEnabled;
		ComputeParams.kernelAlpha = TraceParams.kernelAlpha;
		ComputeParams.viewportRatio = TraceParams.viewportRatio;
		ComputeParams.resoltion = DirectX::XMFLOAT2(ViewportWidth, ViewportHeight);

		//App specific code goes here
		//Feed scene into tracer and tell it to trace the scene

		Camera& SceneCamera = RayScene.SceneCamera;

		float MouseSensitivity = 0.3f;

		if (InputHandler->IsKeyDown(VK_RBUTTON))
		{
			if (!LMouseClicked)
			{
				InputHandler->MouseDelta = Vector2f(0, 0);
				InputHandler->SetCursorToCenter();
			}
			InputHandler->SetCursorVisiblity(false);
			InputHandler->LockMouseToCenter = true;
			SceneCamera.Orientation *= Quaternion(MouseSensitivity * DeltaTime * InputHandler->MouseDelta.X, SceneCamera.BaseUp);
			SceneCamera.Orientation *= Quaternion(MouseSensitivity * DeltaTime * InputHandler->MouseDelta.Y, SceneCamera.BaseRight);
			LMouseClicked = true;
		}
		else
		{
			LMouseClicked = false;
			InputHandler->LockMouseToCenter = false;
			InputHandler->SetCursorVisiblity(true);
		}

		float CameraForwardMove = InputHandler->IsKeyDown(W_KEY) - InputHandler->IsKeyDown(S_KEY);
		float CameraRightMove = InputHandler->IsKeyDown(D_KEY) - InputHandler->IsKeyDown(A_KEY);
		float CameraUpMove = InputHandler->IsKeyDown(E_KEY) - InputHandler->IsKeyDown(Q_KEY);

		float CameraSpeed = 200.0f;

		Vector3f CameraMove = CameraForwardMove * SceneCamera.GetForward() + CameraRightMove * SceneCamera.GetRight() + CameraUpMove * SceneCamera.GetUpVector();

		SceneCamera.Position = SceneCamera.Position + CameraMove * DeltaTime * CameraSpeed;

		ViewportHeight = WindowHeight / ViewportRatio;
		ViewportWidth = WindowWidth / ViewportRatio;
		TraceParams.viewportRatio = ViewportRatio;

		RayTracer.Update(RayScene, TraceParams, ComputeParams);

		//// --- Rendering ---

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		bool opened = ImGui::Begin("Render Time", nullptr, ImGuiWindowFlags_None);
		if (opened)
		{
			ImGui::Text("Frame rate: %.3f fps", FpsRunningAverage);
			ImGui::SliderInt("Sqrt spp", reinterpret_cast<int*>(&TraceParams.sqrtSamplesPerPixel), 0, 10);
			ImGui::SliderFloat("Viewport Ratio", &ViewportRatio, 1.0f, 3.0f);
			ImGui::Separator();
			ImGui::Checkbox("Use foveated rendering", reinterpret_cast<bool*>(&TraceParams.isFoveatedRenderingEnabled));
			ImGui::SliderFloat2("Foveal point", reinterpret_cast<float*>(&TraceParams.fovealCenter), 0.f, 1920.f);
			ImGui::SliderFloat("Kernel Alpha", &TraceParams.kernelAlpha, 0.5f, 6.0f);
			ImGui::SliderFloat("foveationFillOffset", &TraceParams.foveationFillOffset, 0.0f, 10.0f);
			ImGui::Checkbox("Blur output", reinterpret_cast<bool*>(&ComputeParams.shouldBlur));
			ImGui::Checkbox("Vsync", &RayTracer.D3D.Vsync);
			ImGui::Checkbox("Motion View", reinterpret_cast<bool*>(&ComputeParams.isMotionView));
			ImGui::Checkbox("Depth View", reinterpret_cast<bool*>(&ComputeParams.isDepthView));
		}
		ImGui::End();

		ImGui::Render();
		RayTracer.Render();

		auto const FrameEnd = std::chrono::high_resolution_clock::now();

		DeltaTime = std::chrono::duration<float>(FrameEnd - FrameStart).count();
		ElapsedTimeS += DeltaTime;
		FrameCount++;

		float FrameFpsAverage = 1.0f / DeltaTime;

		FpsRunningAverage = FpsRunningAverage * 0.9f + FrameFpsAverage * 0.1f;
		FpsTrueAverage = 1.0f * (static_cast<float>(FrameCount) / ElapsedTimeS);

		if (FrameCount % 200 == 0)
		{
			CORE_TRACE("FpsRunning = {0}, FpsTrue = {1}, FpsCurrent = {2}", FpsRunningAverage, FpsTrueAverage, FrameFpsAverage);
		}

		TraceParams.elapsedTimeSeconds = ElapsedTimeS;

		InputHandler->OnFrameEnd();
	}

	Cleanup();
}

void Application::Cleanup()
{
	ClipCursor(NULL);

	if (InputHandler) delete InputHandler;

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	RayScene.Clear();
	RayTracer.Cleanup();
	DestroyWindow(Window);
}