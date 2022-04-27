#include "pch.h"
#include "Application.h"
#include "Log.h"

#include "imgui/imgui_impl_win32.h"

#include <stdlib.h>
#include <iostream>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT ExitCode = EXIT_SUCCESS;

	Application& App = Application::GetApplication();
	App.Init(1920, 1080, hInstance, L"FOVTracer");
	App.Run();

	return ExitCode;
}

void Application::Init(LONG width, LONG height, HINSTANCE& instance, LPCWSTR title)
{
	HRESULT HR = AppWindow::Create(width, height, instance, Window, title);
	Utils::Validate(HR, L"Error: Failed to create window!");

	HR = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Utils::Validate(HR, L"Error: Failed to initialize COM!");

	RECT OldClip, Clip;

	GetClipCursor(&OldClip);
	GetWindowRect(Window, &Clip);

	AllocConsole();

	Log::Init();

	//RayScene.LoadFromPath(Utils::GetResourcePath("SunTemple/SunTemple.fbx"), false);
	//RayScene.LoadFromPath(Utils::GetResourcePath("cornell_box/CornellBox-Sphere.obj"), false);
	//RayScene.LoadFromPath(Utils::GetResourcePath("sibenik/sibenik.obj"), true);
	//RayScene.LoadFromPath(Utils::GetResourcePath("San_Miguel/san-miguel-low-poly.obj"), false);
	RayScene.LoadFromPath(Utils::GetResourcePath("sponza/sponza.obj"), false);
	//RayScene.LoadFromPath(Utils::GetResourcePath("misc/bunny.obj"), true);
	CORE_INFO("{0} objects in scene.", RayScene.GetNumSceneObjects());

	RayScene.SceneCamera.Orientation *= Quaternion(0.0f, DirectX::XM_PI/2.0f, 0.0f);
	RayScene.SceneCamera.Position = Vector3f(0.0f, 0.0f, 0.0f);
	RayScene.SceneCamera.FOV = 75.0f;

	//Initalize and configure tracer
	TracerConfigInfo Config;
	Config.Height = height;
	Config.Width = width;
	Config.Instance = instance;
	Config.Vsync = false;

	WindowHeight = ViewportHeight = static_cast<float>(height);
	WindowWidth = ViewportWidth = static_cast<float>(width);

	IMGUI_CHECKVERSION();
	UIContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(UIContext);

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Window);

	InputHandler = new Input(ImGui::GetIO(), Window);

	RayTracer.AddTargetResolution(1920, 1080, "1920x1080");
	RayTracer.AddTargetResolution(1280, 720, "1280x720");

	RayTracer.Init(Config, Window, RayScene);

	RayTracer.SetResolution("1920x1080", IsDLSSEnabled, 0, false);
}

void Application::Run()
{
	MSG msg = { 0 };

	TracerParameters TraceParams;
	ComputeParams ComputeParams;

	float DeltaTime = 0.0f;
	float FpsRunningAverage = 0.0f;

	float ViewportRatio = 0.555f;
	bool CustomRenderResolution = true;
	bool LMouseClicked = false;

	float jitterStrength = 1.0f;

	bool scrShotDisableDLSS = false;
	bool scrShotDisableFOV = false;
	bool swayCamera = false;
	bool moveCamera = false;
	float cameraSwaySpeed = 0.1f;
	float cameraMoveSpeed = 0.1f;
	float cameraMoveAmount = 0.1f;
	uint32_t scrShotSqrtSamples = 6;
	uint32_t nScrShot = 1;
	uint32_t scrShotDepth = 1;

	float CameraSpeed = 100.0f;
	bool SetRotationFromUI = false;
	Vector3f CameraEulerRotation;

	float cameraInterpT = 0.0f;

	Camera& SceneCamera = RayScene.SceneCamera;
	Quaternion OriginalCamOrianetation = SceneCamera.Orientation;

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
		ComputeParams.foveationAreaThreshold = TraceParams.foveationAreaThreshold;
		//ComputeParams.resetColorHistory = false;

		if (!IsRecording)
		{
			cameraInterpT = 0;

			float CameraForwardMove = static_cast<float>(InputHandler->IsKeyDown(W_KEY) - InputHandler->IsKeyDown(S_KEY));
			float CameraRightMove = static_cast<float>(InputHandler->IsKeyDown(D_KEY) - InputHandler->IsKeyDown(A_KEY));
			float CameraUpMove = static_cast<float>(InputHandler->IsKeyDown(E_KEY) - InputHandler->IsKeyDown(Q_KEY));

			

			Vector3f CameraMove = CameraForwardMove * SceneCamera.GetForward() + CameraRightMove * SceneCamera.GetRight() + CameraUpMove * SceneCamera.GetUpVector();

			SceneCamera.Position = SceneCamera.Position + CameraMove * DeltaTime * CameraSpeed;

			float MouseSensitivity = 0.001f;

			float CameraRoll = static_cast<float>(InputHandler->IsKeyDown(Z_KEY) - InputHandler->IsKeyDown(X_KEY));
			float RollSensitivity = 2;

			if (InputHandler->IsKeyDown(VK_LBUTTON))
			{
				if (!LMouseClicked)
				{
					InputHandler->MouseDelta = Vector2f(0, 0);
				}
				Vector3f Rotation = Vector3f(MouseSensitivity * InputHandler->MouseDelta.Y, MouseSensitivity * InputHandler->MouseDelta.X, CameraRoll * RollSensitivity * DeltaTime);
				SceneCamera.Orientation *= Quaternion(Rotation.X, Rotation.Y, Rotation.Z);

				LMouseClicked = true;
			}
			else
			{
				LMouseClicked = false;
			}
		}
		else
		{
			cameraInterpT += 1;

			if (swayCamera)
			{
				Vector3f Rotation = Vector3f(0, sin(cameraInterpT * cameraSwaySpeed), 0);
				SceneCamera.Orientation = OriginalCamOrianetation * Quaternion(Rotation.X, Rotation.Y, Rotation.Z);
			}

			if (moveCamera)
			{
				Vector3f Movement = Vector3f(3 * sin(cameraInterpT * cameraMoveSpeed), 0, cos(cameraInterpT * cameraMoveSpeed));
				SceneCamera.Position = SceneCamera.Position + Movement * cameraMoveAmount;
			}
		}

		if (SetRotationFromUI)
		{
			Vector3f Orientation = CameraEulerRotation * (DirectX::XM_2PI / 360.f);
			SceneCamera.Orientation = Quaternion(Orientation.X, Orientation.Y, Orientation.Z);
		}

		RayTracer.Update(RayScene, TraceParams, ComputeParams, jitterStrength);

		//// --- Rendering ---

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		
		bool opened = ImGui::Begin("Render Time", nullptr, ImGuiWindowFlags_None);
		if (opened)
		{
			ImGui::Text("Frame rate: %.3f fps", FpsRunningAverage);
			ImGui::Text("Raytracing time: %.3f ms", RaytraceTimeMS);
			ImGui::Text("DLSS time: %.3f ms", DLSSTimeMS);
			ImGui::SliderInt("Sqrt spp", reinterpret_cast<int*>(&TraceParams.sqrtSamplesPerPixel), 0, 10);
			ImGui::SliderInt("Recursion depth", reinterpret_cast<int*>(&TraceParams.recursionDepth), 1, 4);
			ImGui::Checkbox("Indirect illumination (Expensive!)", reinterpret_cast<bool*>(&TraceParams.useIndirectIllum));
			ImGui::Checkbox("Flip normals", reinterpret_cast<bool*>(&TraceParams.flipNormals));
			ImGui::SliderFloat("Ray T max", &TraceParams.rayTMax, 0.0f, 1000000.0f);
			const char* items[] = { "1920x1080", "1280x720" };
			static const char* current_item = "1920x1080";

			if (ImGui::BeginCombo("##combo", current_item)) // The second parameter is the label previewed before opening the combo.
			{
				for (int n = 0; n < IM_ARRAYSIZE(items); n++)
				{
					bool is_selected = (current_item == items[n]); // You can store your selection however you want, outside or inside your objects
					if (ImGui::Selectable(items[n], is_selected))
					{
						current_item = items[n];
						RayTracer.SetResolution(current_item, IsDLSSEnabled, ViewportRatio, CustomRenderResolution);
						if (is_selected)
						{
							ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
						}
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Separator();
			ImGui::Checkbox("Use foveated rendering", reinterpret_cast<bool*>(&TraceParams.isFoveatedRenderingEnabled));
			ImGui::SliderFloat2("Foveal point", reinterpret_cast<float*>(&TraceParams.fovealCenter), 0.f, 1.f);
			ImGui::SliderFloat("Kernel Alpha", &TraceParams.kernelAlpha, 0.f, 6.0f);
			ImGui::SliderFloat("Foveation threshold", &TraceParams.foveationAreaThreshold, 0.0f, 1.0f);
			ImGui::Checkbox("Vsync", &RayTracer.D3D.Vsync);
			ImGui::Checkbox("Motion View", reinterpret_cast<bool*>(&ComputeParams.isMotionView));
			ImGui::Checkbox("Depth View", reinterpret_cast<bool*>(&ComputeParams.isDepthView));
			ImGui::Checkbox("WorldPos View", reinterpret_cast<bool*>(&ComputeParams.isWorldPosView));

			ImGui::Separator();
			ImGui::Checkbox("DLSS", &IsDLSSEnabled);
			ImGui::Checkbox("Use custom downscale", &CustomRenderResolution);
			ImGui::SliderFloat("Downscale amount", &ViewportRatio, 0.1f, 1.0f);
			ImGui::SliderFloat("Jitter Strength", &jitterStrength, 0.0f, 1.5f);
			ImGui::Checkbox("Disable TAA", reinterpret_cast<bool*>(&ComputeParams.disableTAA));

			ImGui::Separator();
			ImGui::Text("Gaussian Blur");
			ImGui::SliderFloat("Blur Inner K", &ComputeParams.blurKInner, 0.0f, 40.0f);
			ImGui::SliderFloat("Blur Outer K", &ComputeParams.blurKOuter, 0.0f, 20.0f);
			ImGui::SliderFloat("Blur A", &ComputeParams.blurA, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Scene controls");
			ImGui::Checkbox("Manual camera rotation", &SetRotationFromUI);
			ImGui::SliderFloat("Camera control speed", &CameraSpeed, 0.0f, 300.0f);
			ImGui::SliderFloat3("Camera position", reinterpret_cast<float*>(&SceneCamera.Position), -10000.f, 10000.f);
			ImGui::SliderFloat3("Camera rotation", reinterpret_cast<float*>(&CameraEulerRotation), 0.f, 360.f);

			ImGui::Separator();
			ImGui::Text("Screenshot reference parameters");
			ImGui::SliderInt("Screenshot sqrt spp", reinterpret_cast<int*>(&scrShotSqrtSamples), 1, 15);
			ImGui::SliderInt("Number of screenshots", reinterpret_cast<int*>(&nScrShot), 1, 300);
			ImGui::SliderInt("Reference recursion depth", reinterpret_cast<int*>(&scrShotDepth), 1, 4);
			ImGui::Checkbox("Disable DLSS", &scrShotDisableDLSS);
			ImGui::Checkbox("Disable Foveation", &scrShotDisableFOV);
			ImGui::Checkbox("Sway camera", &swayCamera);
			ImGui::SliderFloat("Sway speed", &cameraSwaySpeed, 0.f, 2.0f);
			ImGui::Checkbox("Move camera", &moveCamera);
			ImGui::SliderFloat("Move speed", &cameraMoveSpeed, 0.f, 2.0f);
			ImGui::SliderFloat("Move amount", &cameraMoveAmount, 0.f, 100.0f);
		}
		ImGui::End();

		ImGui::Render();
		RayTracer.Render(InputHandler->IsKeyJustPressed(P_KEY), scrShotSqrtSamples, scrShotDisableFOV, scrShotDisableDLSS, nScrShot, scrShotDepth);

		auto const FrameEnd = std::chrono::high_resolution_clock::now();

		DeltaTime = std::chrono::duration<float>(FrameEnd - FrameStart).count();
		ElapsedTimeS += DeltaTime;

		float FrameFpsAverage = 1.0f / DeltaTime;

		FpsRunningAverage = FpsRunningAverage * 0.9f + FrameFpsAverage * 0.1f;
		TraceParams.elapsedTimeSeconds = ElapsedTimeS;

		FrameCount++;

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

void Application::Resize(LONG newWidth, LONG newHeight)
{
	SetWindowPos(Window, HWND_TOPMOST, 0, 0, newWidth, newHeight, SWP_NOOWNERZORDER | SWP_NOMOVE);
}

