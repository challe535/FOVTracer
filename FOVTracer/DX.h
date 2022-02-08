#pragma once

#include "ResourceManagement.h"
#include "StaticMesh.h"

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <dxc/dxcapi.h>
#include <dxc/dxcapi.use.h>

#include <wrl.h>
#include <atlcomcli.h>


#include <string>
#include <vector>

#define DXR_ENABLED 0
#define NAME_D3D_RESOURCES 1


static const D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0, 0
};

static const D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0, 0
};

struct TextureInfo
{
	std::vector<UINT8> pixels;
	int width = 0;
	int height = 0;
	int stride = 0;
	int offset = 0;
};

struct MaterialCB
{
	DirectX::XMFLOAT4 resolution;
};

struct ViewCB
{
	DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT4 viewOriginAndTanHalfFovY = DirectX::XMFLOAT4(0, 0.f, 0.f, 0.f);
	DirectX::XMFLOAT2 resolution = DirectX::XMFLOAT2(1280, 720);
};

struct D3D12Global
{
	IDXGIFactory4* Factory = nullptr;
	IDXGIAdapter1* Adapter = nullptr;
	ID3D12Device5* Device = nullptr;
	ID3D12GraphicsCommandList4* CmdList = nullptr;
	ID3D12CommandQueue* CmdQueue = nullptr;
	ID3D12CommandAllocator* CmdAlloc[2] = { nullptr, nullptr };

	IDXGISwapChain3* SwapChain = nullptr;
	ID3D12Resource* BackBuffer[2] = { nullptr, nullptr };

	ID3D12Fence* Fence = nullptr;
	UINT64 FenceValues[2] = { 0, 0 };
	HANDLE FenceEvent;
	UINT FrameIndex = 0;

	int Width = 640;
	int	Height = 360;
	bool Vsync = false;
};

struct D3D12ShaderCompilerInfo
{
	dxc::DxcDllSupport DxcDllHelper;
	IDxcCompiler* Compiler = nullptr;
	IDxcLibrary* Library = nullptr;
};

struct D3D12BufferCreateInfo
{
	UINT64 size = 0;
	UINT64 alignment = 0;
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

	D3D12BufferCreateInfo() {}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) : size(InSize), flags(InFlags) {}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
		size(InSize),
		heapType(InHeapType),
		state(InState)
	{
	}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		size(InSize),
		flags(InFlags),
		state(InState)
	{
	}

	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		size(InSize),
		alignment(InAlignment),
		heapType(InHeapType),
		flags(InFlags),
		state(InState)
	{
	}
};

struct D3D12Resources
{
	ID3D12Resource* DXROutput;

	ID3D12Resource* vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW						vertexBufferView;
	ID3D12Resource* indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW							indexBufferView;

	ID3D12Resource* viewCB = nullptr;
	ViewCB											viewCBData;
	UINT8* viewCBStart = nullptr;

	ID3D12Resource* materialCB = nullptr;
	MaterialCB										materialCBData;
	UINT8* materialCBStart = nullptr;

	ID3D12DescriptorHeap* rtvHeap = nullptr;
	ID3D12DescriptorHeap* descriptorHeap = nullptr;

	ID3D12Resource* texture = nullptr;
	ID3D12Resource* textureUploadResource = nullptr;

	UINT											rtvDescSize = 0;

	float											translationOffset = 0;
	float											rotationOffset = 0;
	DirectX::XMFLOAT3								eyeAngle;
	DirectX::XMFLOAT3								eyePosition;
};

struct D3D12ShaderInfo
{
	LPCWSTR		filename = nullptr;
	LPCWSTR		entryPoint = nullptr;
	LPCWSTR		targetProfile = nullptr;
	LPCWSTR* arguments = nullptr;
	DxcDefine* defines = nullptr;
	UINT32		argCount = 0;
	UINT32		defineCount = 0;

	D3D12ShaderInfo() {}
	D3D12ShaderInfo(LPCWSTR inFilename, LPCWSTR inEntryPoint, LPCWSTR inProfile)
	{
		filename = inFilename;
		entryPoint = inEntryPoint;
		targetProfile = inProfile;
	}
};

struct AccelerationStructureBuffer
{
	ID3D12Resource* pScratch = nullptr;
	ID3D12Resource* pResult = nullptr;
	ID3D12Resource* pInstanceDesc = nullptr;	// only used in top-level AS
};

struct RtProgram
{
	D3D12ShaderInfo			info = {};
	IDxcBlob* blob = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;

	D3D12_DXIL_LIBRARY_DESC	dxilLibDesc;
	D3D12_EXPORT_DESC		exportDesc;
	D3D12_STATE_SUBOBJECT	subobject;
	std::wstring			exportName;

	RtProgram()
	{
		exportDesc.ExportToRename = nullptr;
	}

	RtProgram(D3D12ShaderInfo shaderInfo)
	{
		info = shaderInfo;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		exportName = shaderInfo.entryPoint;
		exportDesc.ExportToRename = nullptr;
		exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	}

	void SetBytecode()
	{
		exportDesc.Name = exportName.c_str();

		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibDesc.DXILLibrary.BytecodeLength = blob->GetBufferSize();
		dxilLibDesc.DXILLibrary.pShaderBytecode = blob->GetBufferPointer();

		subobject.pDesc = &dxilLibDesc;
	}

};

struct HitProgram
{
	RtProgram ahs;
	RtProgram chs;

	std::wstring exportName;
	D3D12_HIT_GROUP_DESC desc = {};
	D3D12_STATE_SUBOBJECT subobject = {};

	HitProgram() {}
	HitProgram(LPCWSTR name) : exportName(name)
	{
		desc = {};
		desc.HitGroupExport = exportName.c_str();
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subobject.pDesc = &desc;
	}

	void SetExports(bool anyHit)
	{
		desc.HitGroupExport = exportName.c_str();
		if (anyHit) desc.AnyHitShaderImport = ahs.exportDesc.Name;
		desc.ClosestHitShaderImport = chs.exportDesc.Name;
	}

};

struct DXRGlobal
{
	AccelerationStructureBuffer						TLAS;
	AccelerationStructureBuffer						BLAS;
	uint64_t										tlasSize;

	ID3D12Resource* shaderTable = nullptr;
	uint32_t										shaderTableRecordSize = 0;

	RtProgram										rgs;
	RtProgram										miss;
	HitProgram										hit;

	ID3D12StateObject* rtpso = nullptr;
	ID3D12StateObjectProperties* rtpsoInfo = nullptr;
};

namespace D3DShaders
{
	void Init_Shader_Compiler(D3D12ShaderCompilerInfo& shaderCompiler);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob);
	void Destroy(D3D12ShaderCompilerInfo& shaderCompiler);
}

namespace D3D12
{
	void Create_Device(D3D12Global& d3d);
	void Create_CommandList(D3D12Global& d3d);
	void Create_Command_Queue(D3D12Global& d3d);
	void Create_Command_Allocator(D3D12Global& d3d);
	void Create_CommandList(D3D12Global& d3d);
	void Create_Fence(D3D12Global& d3d);
	void Create_SwapChain(D3D12Global& d3d, HWND& window);

	ID3D12RootSignature* Create_Root_Signature(D3D12Global& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc);

	void Reset_CommandList(D3D12Global& d3d);
	void Submit_CmdList(D3D12Global& d3d);
	void Present(D3D12Global& d3d);
	void WaitForGPU(D3D12Global& d3d);
	void MoveToNextFrame(D3D12Global& d3d);

	void Destroy(D3D12Global& d3d);
}

namespace D3DResources
{
	void Create_Buffer(D3D12Global& d3d, D3D12BufferCreateInfo& info, ID3D12Resource** ppResource);
	void Create_Texture(D3D12Global& d3d, D3D12Resources& resources, Material& material);
	void Create_Vertex_Buffer(D3D12Global& d3d, D3D12Resources& resources, StaticMesh& model);
	void Create_Index_Buffer(D3D12Global& d3d, D3D12Resources& resources, StaticMesh& model);
	void Create_Constant_Buffer(D3D12Global& d3d, ID3D12Resource** buffer, UINT64 size);
	void Create_BackBuffer_RTV(D3D12Global& d3d, D3D12Resources& resources);
	void Create_View_CB(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Material_CB(D3D12Global& d3d, D3D12Resources& resources, const Material& material);
	void Create_Descriptor_Heaps(D3D12Global& d3d, D3D12Resources& resources);

	void Update_View_CB(D3D12Global& d3d, D3D12Resources& resources);

	void Upload_Texture(D3D12Global& d3d, ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture);

	void Destroy(D3D12Resources& resources);
}

namespace DXR
{
	void Create_Bottom_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, StaticMesh& model);
	void Create_Top_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);
	void Create_RayGen_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Closest_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr);
	void Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);
	void Create_Descriptor_Heaps(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, const StaticMesh& model);
	void Create_DXR_Output(D3D12Global& d3d, D3D12Resources& resources);

	void Build_Command_List(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);

	void Destroy(DXRGlobal& dxr);
}