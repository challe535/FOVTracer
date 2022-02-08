#include "Tracer.h"
#include "Log.h"

void Tracer::Init(TracerConfigInfo config, HWND& window, Scene scene) 
{
	D3D.Width = config.Width;
	D3D.Height = config.Height;
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
	D3DResources::Create_Vertex_Buffer(D3D, Resources, scene.SceneObjects[0].StaticMeshes[0]);
	D3DResources::Create_Index_Buffer(D3D, Resources, scene.SceneObjects[0].StaticMeshes[0]);
	D3DResources::Create_Texture(D3D, Resources, scene.SceneObjects[0].StaticMeshes[0].MeshMaterial);
	D3DResources::Create_View_CB(D3D, Resources);
	D3DResources::Create_Material_CB(D3D, Resources, scene.SceneObjects[0].StaticMeshes[0].MeshMaterial);

	// Create DXR specific resources
	DXR::Create_Bottom_Level_AS(D3D, DXR, Resources, scene.SceneObjects[0].StaticMeshes[0]);
	DXR::Create_Top_Level_AS(D3D, DXR, Resources);
	DXR::Create_DXR_Output(D3D, Resources);
	DXR::Create_Descriptor_Heaps(D3D, DXR, Resources, scene.SceneObjects[0].StaticMeshes[0]);
	DXR::Create_RayGen_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Miss_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Closest_Hit_Program(D3D, DXR, ShaderCompiler);
	DXR::Create_Pipeline_State_Object(D3D, DXR);
	DXR::Create_Shader_Table(D3D, DXR, Resources);

	D3D.CmdList->Close();
	ID3D12CommandList* pGraphicsList = { D3D.CmdList };
	D3D.CmdQueue->ExecuteCommandLists(1, &pGraphicsList);

	D3D12::WaitForGPU(D3D);
	D3D12::Reset_CommandList(D3D);
#else
	CORE_WARN("Raytracing is disabled! No rendering will happen until \"DXR_ENALBED\" is set to 1 in DX.h");
#endif
}

void Tracer::Update()
{
#if DXR_ENABLED
	D3DResources::Update_View_CB(D3D, Resources);
#endif
}

void Tracer::Render()
{
#if DXR_ENABLED
	DXR::Build_Command_List(D3D, DXR, Resources);
	D3D12::Present(D3D);
	D3D12::MoveToNextFrame(D3D);
	D3D12::Reset_CommandList(D3D);
#endif
}

void Tracer::Cleanup()
{
#if DXR_ENABLED
	D3D12::WaitForGPU(D3D);
	CloseHandle(D3D.FenceEvent);

	DXR::Destroy(DXR);
	D3DResources::Destroy(Resources);
	D3DShaders::Destroy(ShaderCompiler);
	D3D12::Destroy(D3D);
#endif
}