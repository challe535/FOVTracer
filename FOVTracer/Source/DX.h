#pragma once

#include "ResourceManagement.h"
#include "Scene.h"

#include "imgui/imgui_impl_dx12.h"

#include "dlss/nvsdk_ngx.h"

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxc/dxcapi.h>
#include <dxc/dxcapi.use.h>

#include <wrl.h>
#include <atlcomcli.h>

#include <string>
#include <vector>
#include <unordered_map>

#define NAME_D3D_RESOURCES 1


namespace DX12Constants
{
	constexpr uint32_t descriptors_per_shader = 12;
}

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

static const D3D12_HEAP_PROPERTIES ReadBackProperties =
{
	D3D12_HEAP_TYPE_READBACK,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0, 0
};


struct DLSSConfig
{
	bool ShouldUseDLSS = false;
	NVSDK_NGX_Parameter* Params = nullptr;
	NVSDK_NGX_Handle* DLSSFeature = nullptr;

	Vector2f JitterOffset;
	float sharpness = 0.f;
};

struct TracerParameters
{
	float elapsedTimeSeconds = 0.f;
	uint32_t sqrtSamplesPerPixel = 1;
	DirectX::XMFLOAT2 fovealCenter = DirectX::XMFLOAT2(.5f, .5f);
	uint32_t isFoveatedRenderingEnabled = 0;
	float kernelAlpha = 1.0f;
	float viewportRatio = 1.0f;
	uint32_t isDLSSEnabled = 0;
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
	uint32_t hasDiffuse;
	uint32_t hasNormal;
	uint32_t hasTransparency;
};

struct ViewCB
{
	DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT4 viewOriginAndTanHalfFovY = DirectX::XMFLOAT4(0, 0.f, 0.f, 0.f);
	DirectX::XMFLOAT2 displayResolution = DirectX::XMFLOAT2(10, 10);

	DirectX::XMFLOAT2 jitterOffset = DirectX::XMFLOAT2(0, 0);
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

	int Width = 1280;
	int	Height = 720;

	int DisplayWidth = 1280;
	int DisplayHeight = 720;

	bool Vsync = false;
};

struct ComputeParams
{
	DirectX::XMFLOAT2 fovealCenter = DirectX::XMFLOAT2(.5f, .5f);
	uint32_t isFoveatedRenderingEnabled = 0;
	float kernelAlpha = 1.0f;
	DirectX::XMFLOAT2 resoltion = DirectX::XMFLOAT2(1920, 1080);
	DirectX::XMFLOAT2 jitterOffset = DirectX::XMFLOAT2(0, 0);
	float blurKInner = 30.0f;
	float blurKOuter = 7.0f;
	float blurA = 0.65f;

	uint32_t isMotionView = 0;
	uint32_t isDepthView = 0;
	uint32_t isWorldPosView = 0;

};

struct ComputeProgram
{
	ID3DBlob* csProgram = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;
};

struct D3D12Compute
{
	ID3DBlob* csProgram = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;

	ComputeProgram mipProgram;
	D3D12_STATIC_SAMPLER_DESC mipSampler = {};

	ID3D12Resource* paramCB = nullptr;
	ComputeParams paramCBData;
	UINT8* paramCBStart = nullptr;

	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	ID3D12DescriptorHeap* mipHeap = nullptr;

	ID3D12PipelineState* cps = nullptr;
	ID3D12PipelineState* mipPs = nullptr;
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

struct TextureResource
{
	TextureInfo textureInfo;
	ID3D12Resource* texture = nullptr;
	ID3D12Resource* textureUploadResource = nullptr;
	D3D12_RESOURCE_DESC resourceDesc = {};
};

struct SceneObjectResource
{
	ID3D12Resource* vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	ID3D12Resource* indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW	indexBufferView;

	ID3D12Resource* materialCB = nullptr;
	MaterialCB materialCBData;
	UINT8* materialCBStart = nullptr;



	std::string diffuseTexKey;
	std::string normalTexKey;
	std::string opacityTexKey;
};

struct D3D12Resources
{
	ID3D12Resource* DXROutput;
	ID3D12Resource* MotionOutput;
	ID3D12Resource* FinalMotionOutput;
	ID3D12Resource* WorldPosBuffer;
	ID3D12Resource* Log2CartOutput;

	ID3D12Resource* DLSSDepthInput;
	ID3D12Resource* DLSSOutput;

	ID3D12Resource* OutputReadBack;

	std::vector<SceneObjectResource> sceneObjResources;
	std::unordered_map<std::string, TextureResource> Textures;

	ID3D12Resource* viewCB = nullptr;
	ViewCB viewCBData;
	UINT8* viewCBStart = nullptr;

	ID3D12Resource* paramCB = nullptr;
	TracerParameters paramCBData;
	UINT8* paramCBStart = nullptr;

	ID3D12DescriptorHeap* rtvHeap = nullptr;
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	ID3D12DescriptorHeap* uiHeap = nullptr;
	ID3D12DescriptorHeap* cpuOnlyHeap = nullptr;
	//ID3D12DescriptorHeap* readbackHeap = nullptr;

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
	RtProgram										shadow_miss;

	HitProgram										hit;
	HitProgram										shadow_hit;

	D3D12_STATIC_SAMPLER_DESC rayTextureSampler = {};

	ID3D12StateObject* rtpso = nullptr;
	ID3D12StateObjectProperties* rtpsoInfo = nullptr;
};

namespace D3DShaders
{
	void Init_Shader_Compiler(D3D12ShaderCompilerInfo& shaderCompiler);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob);
	HRESULT CompileComputeShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
		_In_ ID3D12Device* device, _Outptr_ ID3DBlob** blob);
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

	void Create_Compute_Params_CB(D3D12Global& d3d, D3D12Compute& dxComp);
	void Create_Compute_Program(D3D12Global& d3d, D3D12Compute& dxComp);
	void Create_Compute_PipelineState(D3D12Global d3d, D3D12Compute& dxComp);
	void Create_Compute_Heap(D3D12Global& d3d, D3D12Resources& resources, D3D12Compute& dxComp);
	void Create_Compute_Output(D3D12Global& d3d, D3D12Resources& resources);
	void Update_Compute_Params(D3D12Compute& dxComp, ComputeParams& params);

	void Create_MipMap_Compute_Program(D3D12Global& d3d, D3D12Compute& dxComp);
	void Create_MipMap_PipelineState(D3D12Global& d3d, D3D12Compute& dxComp);
	void Create_MipMap_Heap(D3D12Global& d3d, D3D12Compute& dxComp);
}

namespace D3DResources
{
	void Create_Buffer(D3D12Global& d3d, D3D12BufferCreateInfo& info, ID3D12Resource** ppResource);
	void Create_Texture(D3D12Global& d3d, TextureResource& resources, TextureInfo& textureInfo);
	void Create_Vertex_Buffer(D3D12Global& d3d, D3D12Resources& resources, const SceneObject& sceneObj, uint32_t index);
	void Create_Index_Buffer(D3D12Global& d3d, D3D12Resources& resources, const SceneObject& sceneObj, uint32_t index);
	void Create_Constant_Buffer(D3D12Global& d3d, ID3D12Resource** buffer, UINT64 size);
	void Create_BackBuffer_RTV(D3D12Global& d3d, D3D12Resources& resources);
	void Create_View_CB(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Material_CB(D3D12Global& d3d, D3D12Resources& resources, const Material& material, uint32_t index);
	void Create_Params_CB(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Descriptor_Heaps(D3D12Global& d3d, D3D12Resources& resources); //Creates RTV heap
	void Create_UIHeap(D3D12Global& d3d, D3D12Resources& resources);

	void Create_ReadBackResources(D3D12Global& d3d, D3D12Resources& resources);

	void Update_Params_CB(D3D12Resources& resources, TracerParameters& params);
	void Update_View_CB(D3D12Global& d3d, D3D12Resources& resources, Camera& camera, Vector2f& jitterOffset, Vector2f& displayResolution);

	void Upload_Texture(D3D12Global& d3d, ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture);
	void Generate_Mips(TextureResource& textureResource, D3D12Global& d3d, D3D12Compute& dxComp);

	void Destroy(D3D12Resources& resources);
}

namespace DXR
{
	void Create_Bottom_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, Scene& model);
	void Create_Top_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);
	void Create_RayGen_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Closest_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr);
	void Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);

	void Create_Non_Shader_Visible_Heap(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Descriptor_Heaps(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, Scene& scene); // Creates raytracing shader heap

	void Create_DLSS_Output(D3D12Global& d3d, D3D12Resources& resources);
	void Create_DXR_Output(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Shadow_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Create_Shadow_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);
	void Add_Alpha_AnyHit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler);

	void Build_Command_List(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, D3D12Compute& dxComp, DLSSConfig& dlssConfig);

	void Destroy(DXRGlobal& dxr);
}