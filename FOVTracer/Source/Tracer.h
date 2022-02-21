#pragma once

#include "DX.h"
#include "Scene.h"

#include <string>

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
	void Init(TracerConfigInfo config, HWND& window, Scene& scene);
	void Update(Scene& scene);
	void Render();
	void Cleanup();


protected:
	DXRGlobal DXR = {};
	D3D12Global D3D = {};
	D3D12Resources Resources = {};
	D3D12ShaderCompilerInfo ShaderCompiler;

	void AddObject(SceneObject& SceneObj, uint32_t Index);
};
