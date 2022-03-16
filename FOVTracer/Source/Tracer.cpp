#include "pch.h"
#include "Tracer.h"
#include "Log.h"
#include "Utils.h"
#include "Application.h"

#define APP_ID 231313132


void Tracer::Init(TracerConfigInfo& config, HWND& window, Scene& scene) 
{
	D3D.Height = config.Height;
	D3D.Width = config.Width;
	D3D.Vsync = config.Vsync;

	D3DShaders::Init_Shader_Compiler(ShaderCompiler);

#if DXR_ENABLED
	D3D12::Create_Device(D3D);
	D3D12::Create_Command_Queue(D3D);
	D3D12::Create_Command_Allocator(D3D);
	D3D12::Create_Fence(D3D);
	D3D12::Create_SwapChain(D3D, window);
	D3D12::Create_CommandList(D3D);
	D3D12::Reset_CommandList(D3D);

	D3DResources::Create_Descriptor_Heaps(D3D, Resources);
	D3DResources::Create_BackBuffer_RTV(D3D, Resources);
	D3DResources::Create_View_CB(D3D, Resources);
	D3DResources::Create_UIHeap(D3D, Resources);
	D3DResources::Create_Params_CB(D3D, Resources);

	for (int i = 0; i < scene.SceneObjects.size(); i++)
		AddObject(scene.SceneObjects[i], i);

	// Create DXR specific resources
	DXR::Create_Bottom_Level_AS(D3D, DXR, Resources, scene);
	DXR::Create_Top_Level_AS(D3D, DXR, Resources);
	DXR::Create_DXR_Output(D3D, Resources);

	DXR::Create_Non_Shader_Visible_Heap(D3D, Resources);
	DXR::Create_Descriptor_Heaps(D3D, DXR, Resources, scene);

	DXR::Create_RayGen_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Miss_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Closest_Hit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Shadow_Hit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Shadow_Miss_Program(D3D, DXR, ShaderCompiler);
	DXR::Add_Alpha_AnyHit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Pipeline_State_Object(D3D, DXR);
	DXR::Create_Shader_Table(D3D, DXR, Resources);

	D3D12::Create_Compute_Params_CB(D3D, DXCompute);
	D3D12::Create_Compute_Program(D3D, DXCompute);
	D3D12::Create_Compute_PipelineState(D3D, DXCompute);
	D3D12::Create_Compute_Output(D3D, Resources);
	D3D12::Create_Compute_Heap(D3D, Resources, DXCompute);

	D3D.CmdList->Close();
	ID3D12CommandList* pGraphicsList = { D3D.CmdList };
	D3D.CmdQueue->ExecuteCommandLists(1, &pGraphicsList);

	D3D12::WaitForGPU(D3D);
	D3D12::Reset_CommandList(D3D);

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = Resources.uiHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = Resources.uiHeap->GetGPUDescriptorHandleForHeapStart();

	ImGui_ImplDX12_Init(D3D.Device, 2,
		DXGI_FORMAT_R8G8B8A8_UNORM, Resources.uiHeap,
		CPUHandle,
		GPUHandle);

	NVSDK_NGX_Result nvr = NVSDK_NGX_D3D12_Init(APP_ID, L"Logs", D3D.Device);
	Utils::ValidateNGX(nvr, "SDK Init");

	nvr = NVSDK_NGX_D3D12_GetCapabilityParameters(&Params);
	Utils::ValidateNGX(nvr, "GetCapabilityParameters");

	if (CheckDLSSIsSupported())
	{
		IsDLSSAvailable = true;

		for (Resolution Res : TargetRenderResolutions)
		{
			unsigned int optWidth = 0, optHeight = 0, maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
			float sharpness = 0.0f;

			NVSDK_NGX_Result nvr = NGX_DLSS_GET_OPTIMAL_SETTINGS(Params, Res.Width, Res.Height, NVSDK_NGX_PerfQuality_Value_Balanced, &optWidth, &optHeight,
				&maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
			Utils::ValidateNGX(nvr, "DLSS Optimal settings");

			Resolution OptRes;
			OptRes.Name = Res.Name;
			OptRes.Width = optWidth;
			OptRes.Height = optHeight;

			CORE_TRACE("{0} optimal resolution is {1}x{2}", OptRes.Name, OptRes.Width, OptRes.Height);

			OptimalRenderResolutions.insert_or_assign(OptRes.Name, OptRes);
		}
	}

#else
	CORE_WARN("Raytracing is disabled! No rendering will happen until \"DXR_ENALBED\" is set to 1 in DX.h");
#endif
}

void Tracer::Update(Scene& scene, TracerParameters& params, ComputeParams& cParams)
{
#if DXR_ENABLED
	params.viewportRatio = 1920  / D3D.Width;

	cParams.viewportRatio = params.viewportRatio;
	cParams.resoltion = DirectX::XMFLOAT2(D3D.Width, D3D.Height);

	D3DResources::Update_Params_CB(Resources, params);
	D3DResources::Update_View_CB(D3D, Resources, scene.SceneCamera);
	D3D12::Update_Compute_Params(DXCompute, cParams);


	//To change viewport width and height in the future use preset resolutions that have been queried for optimal resolution.
	//D3D.Width = Application::GetApplication().ViewportWidth;
	//D3D.Height = Application::GetApplication().ViewportHeight;
#endif
}

void Tracer::Render()
{
#if DXR_ENABLED
	DXR::Build_Command_List(D3D, DXR, Resources, DXCompute);
	D3D12::Present(D3D);
	D3D12::MoveToNextFrame(D3D);
	D3D12::Reset_CommandList(D3D);
#endif
}

void Tracer::Cleanup()
{
#if DXR_ENABLED
	NVSDK_NGX_D3D12_DestroyParameters(Params);
	D3D12::WaitForGPU(D3D);
	CloseHandle(D3D.FenceEvent);

	DXR::Destroy(DXR);
	D3DResources::Destroy(Resources);
	D3DShaders::Destroy(ShaderCompiler);
	D3D12::Destroy(D3D);
#endif
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

	NewTexture.textureInfo = Utils::LoadTexture(TextureName, Resources);
	D3DResources::Create_Texture(D3D, NewTexture, NewTexture.textureInfo);

	Resources.Textures.insert_or_assign(TextureName, NewTexture);

	return NewTexture;
}

bool Tracer::CheckDLSSIsSupported()
{
	int needsUpdatedDriver = 1;
	unsigned int minDriverVersionMajor = 0;
	unsigned int minDriverVersionMinor = 0;


	NVSDK_NGX_Result ResultUpdatedDriver =
		Params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
	NVSDK_NGX_Result ResultMinDriverVersionMajor =
		Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVersionMajor);
	NVSDK_NGX_Result ResultMinDriverVersionMinor =
		Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVersionMinor);

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

void Tracer::SetResolution(const char* ResolutionName)
{
	std::string NameStr(ResolutionName);

	Resolution OptRes = OptimalRenderResolutions.at(NameStr);

	D3D.Width = OptRes.Width;
	D3D.Height = OptRes.Height;

	TargetRes = TargetRenderResolutionsMap.at(NameStr);
	//Application::GetApplication().Resize(TargetRes.Width, TargetRes.Height);

	CORE_INFO("Set resolution to {0}x{1}", D3D.Width, D3D.Height);
}

void Tracer::AddTargetResolution(unsigned int Width, unsigned int Height, const std::string& Name)
{
	Resolution NewRes;
	NewRes.Name = Name;
	NewRes.Width = Width;
	NewRes.Height = Height;

	TargetRenderResolutions.push_back(NewRes);
	TargetRenderResolutionsMap.insert_or_assign(NewRes.Name, NewRes);
}