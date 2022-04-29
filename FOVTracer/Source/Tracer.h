#pragma once

#include "DX.h"
#include "Scene.h"
#include "dlss/nvsdk_ngx.h"
#include "dlss/nvsdk_ngx_helpers.h"

#include <string>


struct Resolution
{
	std::string Name;
	unsigned int Width = 0;
	unsigned int Height = 0;
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
	void Update(Scene& scene, TracerParameters& params, ComputeParams& cParams, float jitterStrength);
	void Render(bool scrshotRequested, uint32_t groundTruthSqrtSpp, bool disableFOV, bool disableDLSS, uint32_t numberOfScreenshots, uint32_t depth);
	void Cleanup();
	void SetResolution(const char* ResolutionName, bool IsDLSSEnabled, float viewportRatio, bool useViewportRatio);
	void AddTargetResolution(unsigned int Width, unsigned int Height, const std::string& Name);

	D3D12Global D3D = {};
	D3D12Resources Resources = {};

protected:
	DXRGlobal DXR = {};
	D3D12ShaderCompilerInfo ShaderCompiler;
	D3D12Compute DXCompute = {};

	DLSSConfig DLSSConfigInfo;

	void InitRenderPipeline(Scene& scene);
	void PipelineCleanup();
	void InitNGX();
	void InitImGUI();

	void AddObject(SceneObject& SceneObj, uint32_t Index);
	TextureResource LoadTexture(std::string TextureName, bool GenMips = true);

	bool CheckDLSSIsSupported();
	bool CreateDLSSFeature(NVSDK_NGX_PerfQuality_Value Quality, Resolution OptimalRenderSize, Resolution DisplayOutSize, bool EnableSharpening);

	Resolution QueryOptimalResolution(Resolution& Res, NVSDK_NGX_PerfQuality_Value Quality, float& outSharpness);

	std::string DumpFrameToFile(const char* name);

	Resolution TargetRes;
	Scene* SceneToTrace = nullptr;

	std::unordered_map<std::string, Resolution> TargetRenderResolutionsMap;

	bool IsDLSSAvailable = false;
	bool IsDLSSFeatureInitialized = false;

	uint32_t screenshotsLeftToTake = 0;
	uint32_t totalScreenshots = 0;
	bool disableDLSSForScreenShot = false;
	bool disableFOVForScreenShot = false;
};
