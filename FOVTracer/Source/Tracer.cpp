#include "pch.h"
#include "Tracer.h"
#include "Log.h"
#include "Utils.h"
#include "Application.h"

#include <iomanip>
#include <ctime>
#include <sstream>


#define APP_ID 231313132


void Tracer::Init(TracerConfigInfo& config, HWND& window, Scene& scene) 
{
	D3D.Height = config.Height;
	D3D.Width = config.Width;

	D3D.DisplayHeight = config.Height;
	D3D.DisplayWidth = config.Width;

	D3D.Vsync = config.Vsync;

	TargetRes.Width = D3D.Width;
	TargetRes.Height = D3D.Height;

	SceneToTrace = &scene;

	D3DShaders::Init_Shader_Compiler(ShaderCompiler);

	D3D12::Create_Device(D3D);
	D3D12::Create_Command_Queue(D3D);
	D3D12::Create_Command_Allocator(D3D);
	D3D12::Create_Fence(D3D);
	D3D12::Create_SwapChain(D3D, window);
	D3D12::Create_CommandList(D3D);
	D3D12::Reset_CommandList(D3D);

	D3D12::Create_MipMap_Compute_Program(D3D, DXCompute);
	D3D12::Create_MipMap_PipelineState(D3D, DXCompute);
	D3D12::Create_MipMap_Heap(D3D, DXCompute);

	D3DResources::Create_Query_Heap(D3D, Resources);

	for (int i = 0; i < scene.SceneObjects.size(); i++)
		AddObject(scene.SceneObjects[i], i);

	D3DResources::Create_UIHeap(D3D, Resources);

	DXR::Create_DLSS_Output(D3D, Resources);
	D3DResources::Create_ReadBackResources(D3D, Resources);

	DXR::Create_Bottom_Level_AS(D3D, DXR, Resources, scene);
	DXR::Create_Top_Level_AS(D3D, DXR, Resources);

	InitRenderPipeline(scene);
	InitImGUI();
	InitNGX();
}

void Tracer::Update(Scene& scene, TracerParameters& params, ComputeParams& cParams, float jitterStrength)
{
	if (TargetRes.Width == 0 || TargetRes.Height == 0)
	{
		CORE_ERROR("Invalid target resolution: {0}x{1}", TargetRes.Width, TargetRes.Height);
		return;
	}

	//cParams.lastJitterOffset = DirectX::XMFLOAT2(DLSSConfigInfo.JitterOffset.X, DLSSConfigInfo.JitterOffset.Y);

	if (DLSSConfigInfo.ShouldUseDLSS || !cParams.disableTAA)
	{

		float RenderTargetRatio = static_cast<float>(TargetRes.Width) / D3D.Width;
		uint64_t TotalPhase = 8u * static_cast<uint64_t>(ceilf(RenderTargetRatio * RenderTargetRatio));

		uint64_t HaltonIndex = Application::GetApplication().FrameCount % TotalPhase;

		DLSSConfigInfo.JitterOffset.X = Math::haltonF(HaltonIndex, 2.f) - 0.5f;
		DLSSConfigInfo.JitterOffset.Y = Math::haltonF(HaltonIndex, 3.f) - 0.5f;

		DLSSConfigInfo.JitterOffset = DLSSConfigInfo.JitterOffset * jitterStrength;
	}
	else
	{
		DLSSConfigInfo.JitterOffset.X = 0;
		DLSSConfigInfo.JitterOffset.Y = 0;
	}

	params.viewportRatio = 1920  / D3D.Width;
	params.isDLSSEnabled = DLSSConfigInfo.ShouldUseDLSS;

	//cParams.currentBufferIndex = params.outBufferIndex;
	cParams.resoltion = DirectX::XMFLOAT2(D3D.Width, D3D.Height);
	cParams.jitterOffset = DirectX::XMFLOAT2(DLSSConfigInfo.JitterOffset.X, DLSSConfigInfo.JitterOffset.Y);
	cParams.usingDLSS = params.isDLSSEnabled;

	Vector2f displayRes(TargetRes.Width, TargetRes.Height);

	D3DResources::Update_Params_CB(Resources, params);
	D3DResources::Update_View_CB(D3D, Resources, scene.SceneCamera, DLSSConfigInfo.JitterOffset, displayRes);
	D3D12::Update_Compute_Params(DXCompute, cParams);

}

void Tracer::Render(bool scrshotRequested, uint32_t groundTruthSqrtSpp, bool disableFOV, bool disableDLSS, uint32_t numberOfScreenshots)
{
	if (scrshotRequested && screenshotsLeftToTake == 0)
	{
		screenshotsLeftToTake = numberOfScreenshots;
		totalScreenshots = numberOfScreenshots;
		disableDLSSForScreenShot = disableDLSS;
		disableFOVForScreenShot = disableFOV;

		Application::GetApplication().IsRecording = true;

		CORE_WARN("===STARTING NEW CAPTURE===");
	}

	bool isTakingScreenshotThisFrame = screenshotsLeftToTake > 0;

	DXR::Build_Command_List(D3D, DXR, Resources, DXCompute, DLSSConfigInfo, isTakingScreenshotThisFrame, DLSSConfigInfo.ShouldUseDLSS, false);
	D3D12::Present(D3D);
	D3D12::MoveToNextFrame(D3D);
	D3D12::Reset_CommandList(D3D);

	UINT64* pTimestampData = nullptr;
	Resources.TimestampReadBack->Map(0, nullptr, reinterpret_cast<void**>(&pTimestampData));

	UINT64 CmdQueueFreq;
	D3D.CmdQueue->GetTimestampFrequency(&CmdQueueFreq);

	auto& App = Application::GetApplication();
	App.RaytraceTimeMS = App.RaytraceTimeMS * 0.9 + 0.1 * 1000 * (pTimestampData[1] - pTimestampData[0]) / (double)CmdQueueFreq;
	App.DLSSTimeMS = App.DLSSTimeMS * 0.9 + 0.1 * 1000 * (pTimestampData[3] - pTimestampData[2]) / (double)CmdQueueFreq;
	Resources.TimestampReadBack->Unmap(0, nullptr);

	if (isTakingScreenshotThisFrame)
	{
		auto test = DumpFrameToFile("scrshot");

		uint32_t currentSpp = Resources.paramCBData.sqrtSamplesPerPixel;

		D3DResources::Update_SPP(Resources, groundTruthSqrtSpp);

		bool PrevDLSS = DLSSConfigInfo.ShouldUseDLSS && disableDLSSForScreenShot;
		int PrevWidth = D3D.Width;
		int PrevHeight = D3D.Height;
		if (PrevDLSS)
		{
			//Turn off DLSS temporarily
			DLSSConfigInfo.ShouldUseDLSS = false;

			D3D.Width = TargetRes.Width;
			D3D.Height = TargetRes.Height;

			DXCompute.paramCBData.usingDLSS = false;
			DXCompute.paramCBData.resoltion = DirectX::XMFLOAT2(D3D.Width, D3D.Height);

			Resources.paramCBData.isDLSSEnabled = false;
		}

		bool PrevFOVRendering = Resources.paramCBData.isFoveatedRenderingEnabled && disableFOVForScreenShot;
		if (PrevFOVRendering)
		{
			//Turn off foveated rendering temporarily
			Resources.paramCBData.isFoveatedRenderingEnabled = false;
			DXCompute.paramCBData.isFoveatedRenderingEnabled = false;
		}

		bool NeedsToDisableTAA = !DXCompute.paramCBData.disableTAA || PrevDLSS || PrevFOVRendering;
		if (NeedsToDisableTAA)
		{
			DXCompute.paramCBData.disableTAA = true;
		}

		Resources.paramCBData.takingReferenceScreenshot = true;
		DXCompute.paramCBData.takingReferenceScreenshot = true;

		D3DResources::Update_Params_CB(Resources, Resources.paramCBData);
		D3D12::Update_Compute_Params(DXCompute, DXCompute.paramCBData);

		DXR::Build_Command_List(D3D, DXR, Resources, DXCompute, DLSSConfigInfo, isTakingScreenshotThisFrame, PrevDLSS, false);
		D3D12::Reset_CommandList(D3D);
		D3D12::WaitForGPU(D3D);

		auto ref = DumpFrameToFile("scrshot_gt");

		D3DResources::Update_SPP(Resources, currentSpp);

		if (PrevDLSS)
		{
			DLSSConfigInfo.ShouldUseDLSS = true;

			D3D.Width = PrevWidth;
			D3D.Height = PrevHeight;

			DXCompute.paramCBData.usingDLSS = true;
			DXCompute.paramCBData.resoltion = DirectX::XMFLOAT2(D3D.Width, D3D.Height);

			Resources.paramCBData.isDLSSEnabled = true;
		}

		//Turn foveated rendering back on
		if (PrevFOVRendering)
		{
			Resources.paramCBData.isFoveatedRenderingEnabled = true;
			DXCompute.paramCBData.isFoveatedRenderingEnabled = true;
		}

		Resources.paramCBData.takingReferenceScreenshot = false;
		DXCompute.paramCBData.takingReferenceScreenshot = false;

		D3DResources::Update_Params_CB(Resources, Resources.paramCBData);
		D3D12::Update_Compute_Params(DXCompute, DXCompute.paramCBData);

		//Flush TAA history sufficiently to avoid the motion vector transition error when capturing in sequence
		DXR::Build_Command_List(D3D, DXR, Resources, DXCompute, DLSSConfigInfo, false, false, false);
		D3D12::Reset_CommandList(D3D);
		D3D12::WaitForGPU(D3D);

		//Renable TAA if it was disabled earlier
		if (NeedsToDisableTAA)
		{
			DXCompute.paramCBData.disableTAA = false;
			D3D12::Update_Compute_Params(DXCompute, DXCompute.paramCBData);
		}

		//Get TAA back up and running after reset for next frame screenshot
		for (int i = 0; i < 10; i++)
		{
			DXR::Build_Command_List(D3D, DXR, Resources, DXCompute, DLSSConfigInfo, false, false, false);
			D3D12::Reset_CommandList(D3D);
			D3D12::WaitForGPU(D3D);
		}

		std::string cmdLine("../FLIP/flip-cuda.exe --reference ");
		cmdLine.append(ref).append(" --test ").append(test).append(" -d ../ImageDumps/FLIP/");
		std::wstring wcmdLine(cmdLine.begin(), cmdLine.end());

		CORE_TRACE("Running FLIP");
		AppWindow::Startup(L"../FLIP/flip-cuda.exe", wcmdLine.data());

		screenshotsLeftToTake--;

		CORE_TRACE("Screenshots progress: {0}%", (int)(100.0f * (1.0f - (screenshotsLeftToTake / (float)totalScreenshots))));

		if (screenshotsLeftToTake == 0 && Application::GetApplication().IsRecording)
			Application::GetApplication().IsRecording = false;

	}
}

void Tracer::Cleanup()
{
	NVSDK_NGX_D3D12_DestroyParameters(DLSSConfigInfo.Params);
	D3D12::WaitForGPU(D3D);
	CloseHandle(D3D.FenceEvent);

	DXR::Destroy(DXR);
	D3DResources::Destroy(Resources);
	D3DShaders::Destroy(ShaderCompiler);
	D3D12::Destroy(D3D);
}

void Tracer::AddObject(SceneObject& SceneObj, uint32_t Index)
{
	Resources.sceneObjResources.emplace_back();

	Resources.sceneObjResources[Index].diffuseTexKey = SceneObj.Mesh.MeshMaterial.TexturePath;
	Resources.sceneObjResources[Index].normalTexKey = SceneObj.Mesh.MeshMaterial.NormalMapPath;

	D3DResources::Create_Vertex_Buffer(D3D, Resources, SceneObj, Index);
	D3DResources::Create_Index_Buffer(D3D, Resources, SceneObj, Index);

	TextureResource DiffuseTexRes = LoadTexture(SceneObj.Mesh.MeshMaterial.TexturePath);
	SceneObj.Mesh.MeshMaterial.TextureResolution = Vector2f(static_cast<float>(DiffuseTexRes.textureInfo.width), static_cast<float>(DiffuseTexRes.textureInfo.height));

	LoadTexture(SceneObj.Mesh.MeshMaterial.NormalMapPath);

	if (SceneObj.Mesh.HasTransparency)
	{
		Resources.sceneObjResources[Index].opacityTexKey = SceneObj.Mesh.MeshMaterial.OpacityMapPath;
		LoadTexture(SceneObj.Mesh.MeshMaterial.OpacityMapPath);
	}

	D3DResources::Create_Material_CB(D3D, Resources, SceneObj.Mesh.MeshMaterial, Index);
}

TextureResource Tracer::LoadTexture(std::string TextureName)
{
	if (Resources.Textures.count(TextureName) > 0)
		return Resources.Textures.at(TextureName);

	TextureResource NewTexture;

	NewTexture.textureInfo = Utils::LoadTexture(TextureName, Resources, 4);
	D3DResources::Create_Texture(D3D, NewTexture, NewTexture.textureInfo);

	//Generate mipmaps
	D3DResources::Generate_Mips(NewTexture, D3D, DXCompute);

	Resources.Textures.insert_or_assign(TextureName, NewTexture);

	return NewTexture;
}

bool Tracer::CheckDLSSIsSupported()
{
	int needsUpdatedDriver = 1;
	unsigned int minDriverVersionMajor = 0;
	unsigned int minDriverVersionMinor = 0;


	NVSDK_NGX_Result ResultUpdatedDriver =
		DLSSConfigInfo.Params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
	NVSDK_NGX_Result ResultMinDriverVersionMajor =
		DLSSConfigInfo.Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVersionMajor);
	NVSDK_NGX_Result ResultMinDriverVersionMinor =
		DLSSConfigInfo.Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVersionMinor);

	Utils::ValidateNGX(ResultUpdatedDriver, "ResultUpdatedDriver");
	Utils::ValidateNGX(ResultMinDriverVersionMajor, "ResultMinDriverVersionMajor");
	Utils::ValidateNGX(ResultMinDriverVersionMinor, "ResultMinDriverVersionMinor");


	if (NVSDK_NGX_SUCCEED(ResultUpdatedDriver) &&
		NVSDK_NGX_SUCCEED(ResultMinDriverVersionMajor) &&
		NVSDK_NGX_SUCCEED(ResultMinDriverVersionMinor))
	{
		if (needsUpdatedDriver)
		{
			// NVIDIA DLSS cannot be loaded due to outdated driver.
			// Min Driver Version required : minDriverVersionMajor.minDriverVersionMinor
			// Fallback to default AA solution (TAA etc) 
			CORE_ERROR("DLSS cannot be loaded due to outdated driver. Required driver version {0}.{1}", minDriverVersionMajor, minDriverVersionMinor);
		}
		else
		{
			// NVIDIA DLSS Minimum driver version was reported as: 
			// minDriverVersionMajor. minDriverVersionMinor
			CORE_TRACE("DLSS version {0}.{1} available!", minDriverVersionMajor, minDriverVersionMinor);
			return true;
		}
	}
	else
	{
		// NVIDIA DLSS Minimum driver version was not reported.
		CORE_ERROR("DLSS Minimum driver version was not reported");
	}

	return false;
}

void Tracer::SetResolution(const char* ResolutionName, bool IsDLSSEnabled, float viewportRatio, bool useViewportRatio)
{
	DLSSConfigInfo.ShouldUseDLSS = false;
	
	std::string NameStr(ResolutionName);
	TargetRes = TargetRenderResolutionsMap.at(NameStr);

	Resolution RenderRes = TargetRes;
	if (IsDLSSEnabled)
	{
		//NVSDK_NGX_PerfQuality_Value Quality = NVSDK_NGX_PerfQuality_Value_Balanced;
		NVSDK_NGX_PerfQuality_Value Quality = NVSDK_NGX_PerfQuality_Value_MaxQuality;

		if (!useViewportRatio)
		{
			RenderRes = QueryOptimalResolution(TargetRes, Quality, DLSSConfigInfo.sharpness);
		}
		else 
		{
			RenderRes = TargetRes;
			RenderRes.Width *= viewportRatio;
			RenderRes.Height *= viewportRatio;
		}
		
		if(DLSSConfigInfo.DLSSFeature) NVSDK_NGX_D3D12_ReleaseFeature(DLSSConfigInfo.DLSSFeature);
		DLSSConfigInfo.ShouldUseDLSS = CreateDLSSFeature(Quality, RenderRes, TargetRes, true);

		CORE_INFO("DLSS Sharpness set to {0}", DLSSConfigInfo.sharpness);
	}
	
	D3D.Width = RenderRes.Width;
	D3D.Height = RenderRes.Height;

	//Application::GetApplication().Resize(TargetRes.Width, TargetRes.Height);
	
	//InitRenderPipeline(*SceneToTrace);

	CORE_INFO("Set resolution to {0}x{1}", D3D.Width, D3D.Height);
}

void Tracer::AddTargetResolution(unsigned int Width, unsigned int Height, const std::string& Name)
{
	Resolution NewRes;
	NewRes.Name = Name;
	NewRes.Width = Width;
	NewRes.Height = Height;

	TargetRenderResolutionsMap.insert_or_assign(NewRes.Name, NewRes);
}

bool Tracer::CreateDLSSFeature(NVSDK_NGX_PerfQuality_Value Quality, Resolution OptimalRenderSize, Resolution DisplayOutSize, bool EnableSharpening)
{
	if (!IsDLSSAvailable)
	{
		CORE_ERROR("Attempt to InitializeDLSSFeature without NGX being initialized.");
		return false;
	}

	unsigned int CreationNodeMask = 1;
	unsigned int VisibilityNodeMask = 1;
	NVSDK_NGX_Result ResultDLSS = NVSDK_NGX_Result_Fail;


	int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
	//DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
	DlssCreateFeatureFlags |= EnableSharpening ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0;

	NVSDK_NGX_DLSS_Create_Params DlssCreateParams = {};

	//memset(&DlssCreateParams, 0, sizeof(DlssCreateParams));

	DlssCreateParams.Feature.InWidth = OptimalRenderSize.Width;
	DlssCreateParams.Feature.InHeight = OptimalRenderSize.Height;
	DlssCreateParams.Feature.InTargetWidth = DisplayOutSize.Width;
	DlssCreateParams.Feature.InTargetHeight = DisplayOutSize.Height;
	DlssCreateParams.Feature.InPerfQualityValue = Quality;
	DlssCreateParams.InFeatureCreateFlags = DlssCreateFeatureFlags;

	D3D.CmdList->Close();
	D3D12::WaitForGPU(D3D);
	D3D12::Reset_CommandList(D3D);

	ResultDLSS = NGX_D3D12_CREATE_DLSS_EXT(D3D.CmdList, CreationNodeMask, VisibilityNodeMask, &DLSSConfigInfo.DLSSFeature, DLSSConfigInfo.Params, &DlssCreateParams);

	Utils::ValidateNGX(ResultDLSS, "DLSS Feature");

	D3D.CmdList->Close();
	ID3D12CommandList* pGraphicsList = { D3D.CmdList };
	D3D.CmdQueue->ExecuteCommandLists(1, &pGraphicsList);

	D3D12::WaitForGPU(D3D);
	D3D12::Reset_CommandList(D3D);

	IsDLSSFeatureInitialized = true;

	return IsDLSSFeatureInitialized;
}

void Tracer::InitRenderPipeline(Scene& scene)
{
	PipelineCleanup();

	D3DResources::Create_Descriptor_Heaps(D3D, Resources);
	D3DResources::Create_BackBuffer_RTV(D3D, Resources);
	D3DResources::Create_View_CB(D3D, Resources);
	D3DResources::Create_Params_CB(D3D, Resources);

	// Create DXR specific resources
	DXR::Create_DXR_Output(D3D, Resources);
	D3D12::Create_Compute_Output(D3D, Resources);

	//DXR::Create_Non_Shader_Visible_Heap(D3D, Resources);
	DXR::Create_Descriptor_Heaps(D3D, DXR, Resources, scene);

	DXR::Create_RayGen_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Miss_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Closest_Hit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Shadow_Hit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Shadow_Miss_Program(D3D, DXR, ShaderCompiler);
	DXR::Add_Alpha_AnyHit_Program(D3D, DXR, ShaderCompiler);
	DXR::Add_Shadow_AnyHit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Pipeline_State_Object(D3D, DXR);
	DXR::Create_Shader_Table(D3D, DXR, Resources);

	D3D12::Create_Compute_Params_CB(D3D, DXCompute);
	D3D12::Create_Compute_Program(D3D, DXCompute);
	D3D12::Create_Compute_PipelineState(D3D, DXCompute);
	D3D12::Create_Compute_Heap(D3D, Resources, DXCompute);

	D3D.CmdList->Close();
	ID3D12CommandList* pGraphicsList = { D3D.CmdList };
	D3D.CmdQueue->ExecuteCommandLists(1, &pGraphicsList);

	D3D12::WaitForGPU(D3D);
	D3D12::Reset_CommandList(D3D);
}

void Tracer::PipelineCleanup()
{
	//Should probably be handeled in the directx files

	SAFE_RELEASE(Resources.rtvHeap);
	SAFE_RELEASE(D3D.BackBuffer[0]);
	SAFE_RELEASE(D3D.BackBuffer[1]);

	SAFE_RELEASE(Resources.viewCB);
	SAFE_RELEASE(Resources.paramCB);

	for(int i = 0; i < NUM_HISTORY_BUFFER; i++)
		SAFE_RELEASE(Resources.DXROutput[i]);
	SAFE_RELEASE(Resources.Log2CartOutput);
	//SAFE_RELEASE(Resources.cpuOnlyHeap);
	SAFE_RELEASE(Resources.descriptorHeap);

	SAFE_RELEASE(DXR.rgs.blob);
	SAFE_RELEASE(DXR.rgs.pRootSignature);
	SAFE_RELEASE(DXR.miss.blob);
	SAFE_RELEASE(DXR.miss.pRootSignature);
	SAFE_RELEASE(DXR.hit.chs.blob);
	SAFE_RELEASE(DXR.hit.chs.pRootSignature);
	SAFE_RELEASE(DXR.hit.ahs.blob);
	SAFE_RELEASE(DXR.hit.ahs.pRootSignature);
	SAFE_RELEASE(DXR.shadow_miss.blob);
	SAFE_RELEASE(DXR.shadow_miss.pRootSignature);
	SAFE_RELEASE(DXR.shadow_hit.chs.blob);
	SAFE_RELEASE(DXR.shadow_hit.chs.pRootSignature);
	SAFE_RELEASE(DXR.shadow_hit.ahs.blob);
	SAFE_RELEASE(DXR.shadow_hit.ahs.pRootSignature);
	SAFE_RELEASE(DXR.rtpso);
	SAFE_RELEASE(DXR.rtpsoInfo);
	SAFE_RELEASE(DXR.shaderTable);

	SAFE_RELEASE(DXCompute.paramCB);
	SAFE_RELEASE(DXCompute.csProgram);
	SAFE_RELEASE(DXCompute.pRootSignature);
	SAFE_RELEASE(DXCompute.cps);
	SAFE_RELEASE(DXCompute.descriptorHeap);
}


void Tracer::InitNGX()
{
	NVSDK_NGX_Result nvr = NVSDK_NGX_D3D12_Init(APP_ID, L"Logs", D3D.Device);
	Utils::ValidateNGX(nvr, "SDK Init");

	nvr = NVSDK_NGX_D3D12_GetCapabilityParameters(&DLSSConfigInfo.Params);
	Utils::ValidateNGX(nvr, "GetCapabilityParameters");

	IsDLSSAvailable = CheckDLSSIsSupported();
}

Resolution Tracer::QueryOptimalResolution(Resolution& TargetResolution, NVSDK_NGX_PerfQuality_Value Quality, float& outSharpness)
{
	unsigned int optWidth = 0, optHeight = 0, maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
	//float sharpness = 0.0f;

	NVSDK_NGX_Result nvr = NGX_DLSS_GET_OPTIMAL_SETTINGS(DLSSConfigInfo.Params, TargetResolution.Width, TargetResolution.Height, Quality, &optWidth, &optHeight,
		&maxWidth, &maxHeight, &minWidth, &minHeight, &outSharpness);
	Utils::ValidateNGX(nvr, "DLSS Optimal settings");

	Resolution OptRes;
	OptRes.Name = TargetResolution.Name;
	OptRes.Width = optWidth;
	OptRes.Height = optHeight;

	CORE_TRACE("{0} optimal resolution is {1}x{2}", OptRes.Name, OptRes.Width, OptRes.Height);

	return OptRes;
}

void Tracer::InitImGUI()
{
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = Resources.uiHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = Resources.uiHeap->GetGPUDescriptorHandleForHeapStart();

	ImGui_ImplDX12_Init(D3D.Device, 2,
		DXGI_FORMAT_R8G8B8A8_UNORM, Resources.uiHeap,
		CPUHandle,
		GPUHandle);
}


std::string Tracer::DumpFrameToFile(const char* name)
{
	D3D12_RANGE ReadbackRange;
	ReadbackRange.Begin = 0;
	ReadbackRange.End = TargetRes.Width * TargetRes.Height;

	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%d-%m-%Y_%Hh%Mm%Ss");
	auto timeStr = oss.str();

	UINT8* pData{};
	Resources.OutputReadBack->Map(0, &ReadbackRange, reinterpret_cast<void**>(&pData));

	std::string destPath("../ImageDumps/");
	destPath = destPath.append(name).append("_").append(timeStr).append(".png");

	if (Utils::DumpPNG(destPath.c_str(), TargetRes.Width, TargetRes.Height, 4, pData))
		CORE_INFO("Screenshot successfully saved to {0}", destPath);
	else
		CORE_ERROR("Failed to take screenshot!");

	D3D12_RANGE EmptyRange{ 0, 0 };
	Resources.OutputReadBack->Unmap(0, &EmptyRange);

	return destPath;
}