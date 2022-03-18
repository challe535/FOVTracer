#pragma once

#include "DX.h"
#include "Scene.h"
#include "dlss/nvsdk_ngx.h"
#include "dlss/nvsdk_ngx_helpers.h"

#include <string>


struct Resolution
{
	std::string Name;
	unsigned int Width;
	unsigned int Height;
};

struct TracerConfigInfo 
{
	int	Width = 640;
	int Height = 360;
	bool Vsync = false;
	std::string	Model = "";
	HINSTANCE Instance = NULL;
};

class Tracer
{
public:
	void Init(TracerConfigInfo& config, HWND& window, Scene& scene);
	void Update(Scene& scene, TracerParameters& params, ComputeParams& cParams);
	void Render();
	void Cleanup();
	void SetResolution(const char* ResolutionName, bool IsDLSSEnabled);
	void AddTargetResolution(unsigned int Width, unsigned int Height, const std::string& Name);

	D3D12Global D3D = {};
	D3D12Resources Resources = {};

protected:
	DXRGlobal DXR = {};
	D3D12ShaderCompilerInfo ShaderCompiler;
	D3D12Compute DXCompute = {};

	//NVSDK_NGX_Parameter* Params = nullptr;
	//NVSDK_NGX_Handle* DLSSFeature = nullptr;

	DLSSConfig DLSSConfigInfo;

	void InitRenderPipeline(Scene& scene);
	void InitNGX();
	void InitImGUI();

	void AddObject(SceneObject& SceneObj, uint32_t Index);
	TextureResource LoadTexture(std::string TextureName);

	bool CheckDLSSIsSupported();
	bool CreateDLSSFeature(NVSDK_NGX_PerfQuality_Value Quality, Resolution OptimalRenderSize, Resolution DisplayOutSize, bool EnableSharpening);

	Resolution TargetRes;
	Scene* SceneToTrace = nullptr;

	std::unordered_map<std::string, Resolution> OptimalRenderResolutions;
	std::unordered_map<std::string, Resolution> TargetRenderResolutionsMap;
	std::vector<Resolution> TargetRenderResolutions;

	bool IsDLSSAvailable = false;
	bool IsDLSSFeatureInitialized = false;
};
