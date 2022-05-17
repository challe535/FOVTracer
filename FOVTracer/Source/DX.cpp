#include "pch.h"
#include "DX.h"
#include "Utils.h"
#include "Log.h"
#include "Math.h"
#include "Application.h"

namespace D3DShaders
{

	/**
	* Compile an HLSL shader using dxcompiler.
	*/
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob)
	{
		HRESULT hr;
		UINT32 code(0);
		IDxcBlobEncoding* pShaderText(nullptr);

		// Load and encode the shader file
		hr = compilerInfo.Library->CreateBlobFromFile(info.filename, &code, &pShaderText);
		Utils::Validate(hr, L"Error: failed to create blob from shader file!");

		// Create the compiler include handler
		CComPtr<IDxcIncludeHandler> dxcIncludeHandler;
		hr = compilerInfo.Library->CreateIncludeHandler(&dxcIncludeHandler);
		Utils::Validate(hr, L"Error: failed to create include handler");

		// Compile the shader
		IDxcOperationResult* result;
		hr = compilerInfo.Compiler->Compile(
			pShaderText,
			info.filename,
			info.entryPoint,
			info.targetProfile,
			info.arguments,
			info.argCount,
			info.defines,
			info.defineCount,
			dxcIncludeHandler,
			&result);

		Utils::Validate(hr, L"Error: failed to compile shader!");

		// Verify the result
		result->GetStatus(&hr);
		if (FAILED(hr))
		{
			IDxcBlobEncoding* error;
			hr = result->GetErrorBuffer(&error);
			Utils::Validate(hr, L"Error: failed to get shader compiler error buffer!");

			// Convert error blob to a string
			std::vector<char> infoLog(error->GetBufferSize() + 1);
			memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
			infoLog[error->GetBufferSize()] = 0;

			std::string errorMsg = "Shader Compiler Error:\n";
			errorMsg.append(infoLog.data());

			MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
			return;
		}

		hr = result->GetResult(blob);
		Utils::Validate(hr, L"Error: failed to get shader blob result!");
	}

	HRESULT CompileComputeShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
		_In_ ID3D12Device* device, _Outptr_ ID3DBlob** blob)
	{
		if (!srcFile || !entryPoint || !device || !blob)
			return E_INVALIDARG;

		*blob = nullptr;

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
		flags |= D3DCOMPILE_DEBUG;
#endif
		// We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance on 11-class hardware
		LPCSTR profile = "cs_5_0";

		const D3D_SHADER_MACRO defines[] =
		{
			NULL, NULL
		};

		ID3DBlob* shaderBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;
		HRESULT hr = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, profile,
			flags, 0, &shaderBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}

			if (shaderBlob)
				shaderBlob->Release();

			return hr;
		}

		*blob = shaderBlob;

		return hr;
	}

	/**
	* Compile an HLSL ray tracing shader using dxcompiler.
	*/
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program)
	{
		Compile_Shader(compilerInfo, program.info, &program.blob);
		program.SetBytecode();
	}

	/**
	* Initialize the shader compiler.
	*/
	void Init_Shader_Compiler(D3D12ShaderCompilerInfo& shaderCompiler)
	{
		HRESULT hr = shaderCompiler.DxcDllHelper.Initialize();
		Utils::Validate(hr, L"Failed to initialize DxCDllSupport!");

		hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompiler.Compiler);
		Utils::Validate(hr, L"Failed to create DxcCompiler!");

		hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &shaderCompiler.Library);
		Utils::Validate(hr, L"Failed to create DxcLibrary!");
	}

	/**
	 * Release shader compiler resources.
	 */
	void Destroy(D3D12ShaderCompilerInfo& shaderCompiler)
	{
		SAFE_RELEASE(shaderCompiler.Compiler);
		SAFE_RELEASE(shaderCompiler.Library);
		shaderCompiler.DxcDllHelper.Cleanup();
	}

}

namespace D3D12
{
	/**
	* Create the device.
	*/
	void Create_Device(D3D12Global& d3d)
	{
#if defined(_DEBUG)
		// Enable the D3D12 debug layer.
		{
			ID3D12Debug* debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
			}
		}
#endif
		// Create a DXGI Factory
		HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&d3d.Factory));
		Utils::Validate(hr, L"Error: failed to create DXGI factory!");

		// Create the device
		d3d.Adapter = nullptr;
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != d3d.Factory->EnumAdapters1(adapterIndex, &d3d.Adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			d3d.Adapter->GetDesc1(&adapterDesc);

			if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;	// Don't select the Basic Render Driver adapter.
			}

			if (SUCCEEDED(D3D12CreateDevice(d3d.Adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device5), (void**)&d3d.Device)))
			{
				// Check if the device supports ray tracing.
				D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
				HRESULT hr = d3d.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));
				if (FAILED(hr) || features.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
				{
					SAFE_RELEASE(d3d.Device);
					d3d.Device = nullptr;
					continue;
				}

#if NAME_D3D_RESOURCES
				d3d.Device->SetName(L"DXR Enabled Device");
				printf("Running on DXGI Adapter %S\n", adapterDesc.Description);
#endif
				break;
			}
		}

		if (d3d.Device == nullptr)
		{
			// Didn't find a device that supports ray tracing.
			Utils::Validate(E_FAIL, L"Error: failed to create ray tracing device!");
		}
	}

	/**
	* Create the command queue.
	*/
	void Create_Command_Queue(D3D12Global& d3d)
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		HRESULT hr = d3d.Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d.CmdQueue));
		Utils::Validate(hr, L"Error: failed to create command queue!");
#if NAME_D3D_RESOURCES
		d3d.CmdQueue->SetName(L"D3D12 Command Queue");
#endif
	}

	/**
	* Create the command allocator for each frame.
	*/
	void Create_Command_Allocator(D3D12Global& d3d)
	{
		for (UINT n = 0; n < 2; n++)
		{
			HRESULT hr = d3d.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d.CmdAlloc[n]));
			Utils::Validate(hr, L"Error: failed to create the command allocator!");
#if NAME_D3D_RESOURCES
			if (n == 0) d3d.CmdAlloc[n]->SetName(L"D3D12 Command Allocator 0");
			else d3d.CmdAlloc[n]->SetName(L"D3D12 Command Allocator 1");
#endif
		}
	}

	/**
	* Create the command list.
	*/
	void Create_CommandList(D3D12Global& d3d)
	{
		HRESULT hr = d3d.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.CmdAlloc[d3d.FrameIndex], nullptr, IID_PPV_ARGS(&d3d.CmdList));
		hr = d3d.CmdList->Close();
		Utils::Validate(hr, L"Error: failed to create the command list!");
#if NAME_D3D_RESOURCES
		d3d.CmdList->SetName(L"D3D12 Command List");
#endif
	}

	/**
	* Create a fence.
	*/
	void Create_Fence(D3D12Global& d3d)
	{
		HRESULT hr = d3d.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d.Fence));
		Utils::Validate(hr, L"Error: failed to create fence!");
#if NAME_D3D_RESOURCES
		d3d.Fence->SetName(L"D3D12 Fence");
#endif

		d3d.FenceValues[d3d.FrameIndex]++;

		// Create an event handle to use for frame synchronization
		d3d.FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		if (d3d.FenceEvent == nullptr)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			Utils::Validate(hr, L"Error: failed to create fence event!");
		}
	}

	/**
	* Create the swap chain.
	*/
	void Create_SwapChain(D3D12Global& d3d, HWND& window)
	{
		// Describe the swap chain
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.BufferCount = 2;
		desc.Width = d3d.Width;
		desc.Height = d3d.Height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.SampleDesc.Count = 1;
		desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		// Create the swap chain
		IDXGISwapChain1* swapChain;
		HRESULT hr = d3d.Factory->CreateSwapChainForHwnd(d3d.CmdQueue, window, &desc, nullptr, nullptr, &swapChain);
		Utils::Validate(hr, L"Error: failed to create swap chain!");

		// Associate the swap chain with a window
		hr = d3d.Factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
		Utils::Validate(hr, L"Error: failed to make window association!");

		// Get the swap chain interface
		hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&d3d.SwapChain));
		Utils::Validate(hr, L"Error: failed to cast swap chain!");

		SAFE_RELEASE(swapChain);
		d3d.FrameIndex = d3d.SwapChain->GetCurrentBackBufferIndex();
	}

	/**
	* Create a root signature.
	*/
	ID3D12RootSignature* Create_Root_Signature(D3D12Global& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		ID3DBlob* sig;
		ID3DBlob* error;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
		Utils::Validate(hr, L"Error: failed to serialize root signature!");

		ID3D12RootSignature* pRootSig = nullptr;

		hr = d3d.Device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
		Utils::Validate(hr, L"Error: failed to create root signature!");

		SAFE_RELEASE(sig);
		SAFE_RELEASE(error);
		return pRootSig;
	}

	ID3D12RootSignature* Create_Versioned_Root_Signature(D3D12Global& d3d, const D3D12_ROOT_SIGNATURE_DESC1& desc)
	{
		ID3DBlob* sig;
		ID3DBlob* error;
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC vSigDesc = {};
		vSigDesc.Desc_1_1 = desc;
		vSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

		HRESULT hr = D3D12SerializeVersionedRootSignature(&vSigDesc, &sig, &error);
		Utils::Validate(hr, L"Error: failed to serialize root signature!");

		ID3D12RootSignature* pRootSig = nullptr;

		hr = d3d.Device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
		Utils::Validate(hr, L"Error: failed to create root signature!");

		SAFE_RELEASE(sig);
		SAFE_RELEASE(error);
		return pRootSig;
	}

	/**
	* Reset the command list.
	*/
	void Reset_CommandList(D3D12Global& d3d)
	{
		// Reset the command allocator for the current frame
		HRESULT hr = d3d.CmdAlloc[d3d.FrameIndex]->Reset();
		Utils::Validate(hr, L"Error: failed to reset command allocator!");

		// Reset the command list for the current frame
		hr = d3d.CmdList->Reset(d3d.CmdAlloc[d3d.FrameIndex], nullptr);
		Utils::Validate(hr, L"Error: failed to reset command list!");
	}

	/*
* Submit the command list.
*/
	void Submit_CmdList(D3D12Global& d3d)
	{
		d3d.CmdList->Close();

		ID3D12CommandList* pGraphicsList = { d3d.CmdList };
		d3d.CmdQueue->ExecuteCommandLists(1, &pGraphicsList);
		d3d.FenceValues[d3d.FrameIndex]++;
		d3d.CmdQueue->Signal(d3d.Fence, d3d.FenceValues[d3d.FrameIndex]);
	}

	/**
	 * Swap the buffers.
	 */
	void Present(D3D12Global& d3d)
	{
		HRESULT hr = d3d.SwapChain->Present(d3d.Vsync ? 1 : 0, !d3d.Vsync ? DXGI_PRESENT_ALLOW_TEARING : 0);
		if (FAILED(hr))
		{
			hr = d3d.Device->GetDeviceRemovedReason();
			Utils::Validate(hr, L"Error: failed to present!");
		}
	}

	/*
	* Wait for pending GPU work to complete.
	*/
	void WaitForGPU(D3D12Global& d3d)
	{
		// Schedule a signal command in the queue
		HRESULT hr = d3d.CmdQueue->Signal(d3d.Fence, d3d.FenceValues[d3d.FrameIndex]);
		Utils::Validate(hr, L"Error: failed to signal fence!");

		// Wait until the fence has been processed
		hr = d3d.Fence->SetEventOnCompletion(d3d.FenceValues[d3d.FrameIndex], d3d.FenceEvent);
		Utils::Validate(hr, L"Error: failed to set fence event!");

		WaitForSingleObjectEx(d3d.FenceEvent, INFINITE, FALSE);

		// Increment the fence value for the current frame
		d3d.FenceValues[d3d.FrameIndex]++;
	}

	/**
	* Prepare to render the next frame.
	*/
	void MoveToNextFrame(D3D12Global& d3d)
	{
		// Schedule a Signal command in the queue
		const UINT64 currentFenceValue = d3d.FenceValues[d3d.FrameIndex];
		HRESULT hr = d3d.CmdQueue->Signal(d3d.Fence, currentFenceValue);
		Utils::Validate(hr, L"Error: failed to signal command queue!");

		// Update the frame index
		d3d.FrameIndex = d3d.SwapChain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is
		if (d3d.Fence->GetCompletedValue() < d3d.FenceValues[d3d.FrameIndex])
		{
			hr = d3d.Fence->SetEventOnCompletion(d3d.FenceValues[d3d.FrameIndex], d3d.FenceEvent);
			Utils::Validate(hr, L"Error: failed to set fence value!");

			WaitForSingleObjectEx(d3d.FenceEvent, INFINITE, FALSE);
		}

		// Set the fence value for the next frame
		d3d.FenceValues[d3d.FrameIndex] = currentFenceValue + 1;
	}

	/**
	 * Release D3D12 resources.
	 */
	void Destroy(D3D12Global& d3d)
	{
		SAFE_RELEASE(d3d.Fence);
		SAFE_RELEASE(d3d.BackBuffer[1]);
		SAFE_RELEASE(d3d.BackBuffer[0]);
		SAFE_RELEASE(d3d.SwapChain);
		SAFE_RELEASE(d3d.CmdAlloc[0]);
		SAFE_RELEASE(d3d.CmdAlloc[1]);
		SAFE_RELEASE(d3d.CmdQueue);
		SAFE_RELEASE(d3d.CmdList);
		SAFE_RELEASE(d3d.Device);
		SAFE_RELEASE(d3d.Adapter);
		SAFE_RELEASE(d3d.Factory);
	}

	void Create_Compute_Params_CB(D3D12Global& d3d, D3D12Compute& dxComp)
	{
		D3DResources::Create_Constant_Buffer(d3d, &dxComp.paramCB, sizeof(ComputeParams));
#if NAME_D3D_RESOURCES
		dxComp.paramCB->SetName(L"Trace Parameters Constant Buffer");
#endif
		HRESULT hr = dxComp.paramCB->Map(0, nullptr, reinterpret_cast<void**>(&dxComp.paramCBStart));
		Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

		memcpy(dxComp.paramCBStart, &dxComp.paramCBData, sizeof(ComputeParams));
	}

	void Create_Compute_PipelineState(D3D12Global d3d, D3D12Compute& dxComp)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = dxComp.pRootSignature;
		D3D12_SHADER_BYTECODE byteCode;
		byteCode.pShaderBytecode = dxComp.csProgram->GetBufferPointer();
		byteCode.BytecodeLength = dxComp.csProgram->GetBufferSize();
		computePsoDesc.CS = byteCode;

		d3d.Device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&dxComp.cps));

		//Watermark program
		computePsoDesc = {};
		computePsoDesc.pRootSignature = dxComp.watermarkProgram.pRootSignature;
		byteCode.pShaderBytecode = dxComp.watermarkProgram.csProgram->GetBufferPointer();
		byteCode.BytecodeLength = dxComp.watermarkProgram.csProgram->GetBufferSize();
		computePsoDesc.CS = byteCode;

		d3d.Device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&dxComp.wmPs));
	}

	void Create_Compute_Program(D3D12Global& d3d, D3D12Compute& dxComp)
	{
		HRESULT hr = D3DShaders::CompileComputeShader(L"Shaders\\RemapCS.hlsl", "CSMain", d3d.Device, &dxComp.csProgram);
		Utils::Validate(hr, L"Failed to compile compute shader");

		D3D12_DESCRIPTOR_RANGE ranges[2];

		ranges[0].BaseShaderRegister = 0;
		ranges[0].NumDescriptors = 1;
		ranges[0].RegisterSpace = 0;
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[0].OffsetInDescriptorsFromTableStart = 0;

		ranges[1].BaseShaderRegister = 0;
		ranges[1].NumDescriptors = 7 + NUM_HISTORY_BUFFER;
		ranges[1].RegisterSpace = 0;
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[1].OffsetInDescriptorsFromTableStart = ranges[0].NumDescriptors;

		D3D12_ROOT_PARAMETER param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		param.DescriptorTable.NumDescriptorRanges = 2;
		param.DescriptorTable.pDescriptorRanges = ranges;

		D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.NumParameters = 1;
		rootDesc.pParameters = &param;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

		dxComp.pRootSignature = D3D12::Create_Root_Signature(d3d, rootDesc);

		//watermark program
		hr = D3DShaders::CompileComputeShader(L"Shaders\\WatermarkCS.hlsl", "CSMain", d3d.Device, &dxComp.watermarkProgram.csProgram);
		Utils::Validate(hr, L"Failed to compile watermark compute shader");

		D3D12_DESCRIPTOR_RANGE range = {};
		range.BaseShaderRegister = 0;
		range.NumDescriptors = 1;
		range.RegisterSpace = 0;
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		range.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER descParam = {};
		descParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		descParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		descParam.DescriptorTable.NumDescriptorRanges = 1;
		descParam.DescriptorTable.pDescriptorRanges = &range;

		rootDesc = {};
		rootDesc.NumParameters = 1;
		rootDesc.pParameters = &descParam;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

		dxComp.watermarkProgram.pRootSignature = D3D12::Create_Root_Signature(d3d, rootDesc);
	}

	void Create_Compute_Heap(D3D12Global& d3d, D3D12Resources& resources, D3D12Compute& dxComp)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 8 + NUM_HISTORY_BUFFER;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dxComp.descriptorHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

		D3D12_CPU_DESCRIPTOR_HANDLE handle = dxComp.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		UINT handleIncrement = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		//Create Params buffer CBV
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(dxComp.paramCBData));

		cbvDesc.BufferLocation = dxComp.paramCB->GetGPUVirtualAddress();

		d3d.Device->CreateConstantBufferView(&cbvDesc, handle);
		handle.ptr += handleIncrement;

		// Create the Color input buffer UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < NUM_HISTORY_BUFFER; i++)
		{
			d3d.Device->CreateUnorderedAccessView(resources.DXROutput[i], nullptr, &uavDesc, handle);
			handle.ptr += handleIncrement;
		}

		// Create the Remapped output buffer UAV
		d3d.Device->CreateUnorderedAccessView(resources.Log2CartOutput, nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		// Create the Motion input buffer UAV
		d3d.Device->CreateUnorderedAccessView(resources.MotionOutput[0], nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		d3d.Device->CreateUnorderedAccessView(resources.MotionOutput[1], nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		// Create the final remapped motion output buffer UAV
		d3d.Device->CreateUnorderedAccessView(resources.FinalMotionOutput, nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		// Create the WorldPosAndDepth input buffer UAV
		d3d.Device->CreateUnorderedAccessView(resources.WorldPosBuffer[0], nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		d3d.Device->CreateUnorderedAccessView(resources.WorldPosBuffer[1], nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		// Create the Depth output buffer UAV
		d3d.Device->CreateUnorderedAccessView(resources.DLSSDepthInput, nullptr, &uavDesc, handle);
		handle.ptr += handleIncrement;

		//Watermark heap
		desc = {};
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dxComp.wmHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

		handle = dxComp.wmHeap->GetCPUDescriptorHandleForHeapStart();
		handleIncrement = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		d3d.Device->CreateUnorderedAccessView(resources.DLSSOutput, nullptr, &uavDesc, handle);
	}

	void Create_Compute_Output(D3D12Global& d3d, D3D12Resources& resources)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = d3d.Width;
		desc.Height = d3d.Height;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		// Create the buffer resource
		HRESULT hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.Log2CartOutput));
		Utils::Validate(hr, L"Error: failed to create remap output buffer!");
//#if NAME_D3D_RESOURCES
//		resources.DXROutput->SetName(L"DXR Output Buffer");
//#endif
		desc.Format = DXGI_FORMAT_R32G32_FLOAT;
		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.FinalMotionOutput));
		Utils::Validate(hr, L"Error: failed to create final motion output buffer!");

		desc.Format = DXGI_FORMAT_R32_FLOAT;
		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.DLSSDepthInput));
		Utils::Validate(hr, L"Error: failed to create depth buffer!");
	}

	void Update_Compute_Params(D3D12Compute& dxComp, ComputeParams& params)
	{
		dxComp.paramCBData = params;
		memcpy(dxComp.paramCBStart, &dxComp.paramCBData, sizeof(dxComp.paramCBData));
	}

	void Create_MipMap_Compute_Program(D3D12Global& d3d, D3D12Compute& dxComp)
	{
		HRESULT hr = D3DShaders::CompileComputeShader(L"Shaders\\MipMapCS.hlsl", "GenerateMipMaps", d3d.Device, &dxComp.mipProgram.csProgram);
		Utils::Validate(hr, L"Failed to compile compute shader");

		D3D12_DESCRIPTOR_RANGE1 ranges[2];

		ranges[0].BaseShaderRegister = 0;
		ranges[0].NumDescriptors = 1;
		ranges[0].RegisterSpace = 0;
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[0].OffsetInDescriptorsFromTableStart = 0;
		ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		ranges[1].BaseShaderRegister = 0;
		ranges[1].NumDescriptors = 1;
		ranges[1].RegisterSpace = 0;
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[1].OffsetInDescriptorsFromTableStart = ranges[0].NumDescriptors;
		ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		//D3D12_ROOT_PARAMETER1 rangesParam = {};
		//rangesParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		//rangesParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		//rangesParam.DescriptorTable.NumDescriptorRanges = 2;
		//rangesParam.DescriptorTable.pDescriptorRanges = ranges;

		D3D12_ROOT_PARAMETER1 srvParam = {};
		srvParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		srvParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		srvParam.DescriptorTable.NumDescriptorRanges = 1;
		srvParam.DescriptorTable.pDescriptorRanges = ranges;

		D3D12_ROOT_PARAMETER1 uavParam = {};
		uavParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		uavParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		uavParam.DescriptorTable.NumDescriptorRanges = 1;
		uavParam.DescriptorTable.pDescriptorRanges = &ranges[1];

		D3D12_ROOT_PARAMETER1 constantsParam = {};
		constantsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		constantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		constantsParam.Constants.Num32BitValues = 3;
		constantsParam.Constants.RegisterSpace = 0;
		constantsParam.Constants.ShaderRegister = 0;

		//D3D12_ROOT_PARAMETER1 params[2] = { constantsParam, rangesParam };
		D3D12_ROOT_PARAMETER1 params[3] = { constantsParam, srvParam, uavParam };

		dxComp.mipSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		dxComp.mipSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		dxComp.mipSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		dxComp.mipSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		dxComp.mipSampler.MipLODBias = 0.0f;
		dxComp.mipSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		dxComp.mipSampler.MinLOD = 0.0f;
		dxComp.mipSampler.MaxLOD = D3D12_FLOAT32_MAX;
		dxComp.mipSampler.MaxAnisotropy = 0;
		dxComp.mipSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		dxComp.mipSampler.ShaderRegister = 0;
		dxComp.mipSampler.RegisterSpace = 0;
		dxComp.mipSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC1 rootDesc = {};
		rootDesc.NumParameters = _countof(params);
		rootDesc.pParameters = params;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		rootDesc.NumStaticSamplers = 1;
		rootDesc.pStaticSamplers = &dxComp.mipSampler;

		dxComp.mipProgram.pRootSignature = D3D12::Create_Versioned_Root_Signature(d3d, rootDesc);
	}

	void Create_MipMap_PipelineState(D3D12Global& d3d, D3D12Compute& dxComp)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = dxComp.mipProgram.pRootSignature;
		D3D12_SHADER_BYTECODE byteCode;
		byteCode.pShaderBytecode = dxComp.mipProgram.csProgram->GetBufferPointer();
		byteCode.BytecodeLength = dxComp.mipProgram.csProgram->GetBufferSize();
		computePsoDesc.CS = byteCode;

		d3d.Device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&dxComp.mipPs));
	}

	void Create_MipMap_Heap(D3D12Global& d3d, D3D12Compute& dxComp)
	{

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 2;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dxComp.mipHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

		//To be filled during mipmap generation
	}
}

namespace D3DResources
{

	/**
	* Create a GPU buffer resource.
	*/
	void Create_Buffer(D3D12Global& d3d, D3D12BufferCreateInfo& info, ID3D12Resource** ppResource)
	{
		D3D12_HEAP_PROPERTIES heapDesc = {};
		heapDesc.Type = info.heapType;
		heapDesc.CreationNodeMask = 1;
		heapDesc.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = info.alignment;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Width = info.size;
		resourceDesc.Flags = info.flags;

		// Create the GPU resource
		HRESULT hr = d3d.Device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, info.state, nullptr, IID_PPV_ARGS(ppResource));
		Utils::Validate(hr, L"Error: failed to create buffer resource!");
	}

	/**
	* Create a texture.
	*/
	void Create_Texture(D3D12Global& d3d, TextureResource& textureResource, TextureInfo& textureInfo)
	{
		// Describe the texture
		textureResource.resourceDesc.Width = textureInfo.width;
		textureResource.resourceDesc.Height = textureInfo.height;
		textureResource.resourceDesc.MipLevels = static_cast<UINT>(floor(log2(fminf(
													static_cast<float>(textureResource.textureInfo.height),
													static_cast<float>(textureResource.textureInfo.width))))
													+ 1);
		textureResource.resourceDesc.DepthOrArraySize = 1;
		textureResource.resourceDesc.SampleDesc.Count = 1;
		textureResource.resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureResource.resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureResource.resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		textureResource.resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		// Create the texture resource
		HRESULT hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureResource.resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource.texture));
		Utils::Validate(hr, L"Error: failed to create texture!");
#if NAME_D3D_RESOURCES
		textureResource.texture->SetName(L"Texture");
#endif

		// Describe the resource
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Width = textureInfo.height * textureInfo.width * textureInfo.stride;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

		// Create the upload heap
		hr = d3d.Device->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textureResource.textureUploadResource));
		Utils::Validate(hr, L"Error: failed to create texture upload heap!");
#if NAME_D3D_RESOURCES
		textureResource.textureUploadResource->SetName(L"Texture Upload Buffer");
#endif

		// Upload the texture to the GPU
		Upload_Texture(d3d, textureResource.texture, textureResource.textureUploadResource, textureInfo);
	}

	/**
	 * Copy a texture from the CPU to the GPU upload heap, then schedule a copy to the default heap.
	 */
	void Upload_Texture(D3D12Global& d3d, ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture)
	{
		// Copy the pixel data to the upload heap resource
		UINT8* pData;
		HRESULT hr = srcResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
		memcpy(pData, texture.pixels.data(), texture.width * texture.height * texture.stride);
		srcResource->Unmap(0, nullptr);

		// Describe the upload heap resource location for the copy
		D3D12_SUBRESOURCE_FOOTPRINT subresource = {};
		subresource.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		subresource.Width = texture.width;
		subresource.Height = texture.height;
		subresource.RowPitch = /*ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT,*/ texture.width * texture.stride/*)*/;
		subresource.Depth = 1;

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		footprint.Offset = texture.offset;
		footprint.Footprint = subresource;

		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = srcResource;
		source.PlacedFootprint = footprint;
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		// Describe the default heap resource location for the copy
		D3D12_TEXTURE_COPY_LOCATION destination = {};
		destination.pResource = destResource;
		destination.SubresourceIndex = 0;
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		// Copy the buffer resource from the upload heap to the texture resource on the default heap
		d3d.CmdList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

		// Transition the texture to a shader resource
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = destResource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		d3d.CmdList->ResourceBarrier(1, &barrier);
	}

	void Generate_Mips(TextureResource& textureResource, D3D12Global& d3d, D3D12Compute& dxComp)
	{
		//Calculate mipmap levels from texture resolution, assume power of 2 resolution for now
		int MaxMipLevels = textureResource.resourceDesc.MipLevels;

		D3D12::Submit_CmdList(d3d);
		D3D12::WaitForGPU(d3d);
		D3D12::Reset_CommandList(d3d);

		SAFE_RELEASE(dxComp.mipHeap);

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = MaxMipLevels;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dxComp.mipHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

		D3D12_CPU_DESCRIPTOR_HANDLE CPUhandle = dxComp.mipHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE GPUhandle = dxComp.mipHeap->GetGPUDescriptorHandleForHeapStart();
		UINT handleIncrement = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		//Prepare the shader resource view description for the source texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSRVDesc = {};
		srcTextureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srcTextureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srcTextureSRVDesc.Format = textureResource.resourceDesc.Format;
		srcTextureSRVDesc.Texture2D.MipLevels = -1;
		srcTextureSRVDesc.Texture2D.MostDetailedMip = 0;
		d3d.Device->CreateShaderResourceView(textureResource.texture, &srcTextureSRVDesc, CPUhandle);
		CPUhandle.ptr += handleIncrement;

		//Prepare the unordered access view description for the destination texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUAVDesc = {};
		destTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		destTextureUAVDesc.Format = textureResource.resourceDesc.Format;

		//Set root signature, pso and descriptor heap
		d3d.CmdList->SetComputeRootSignature(dxComp.mipProgram.pRootSignature);
		d3d.CmdList->SetPipelineState(dxComp.mipPs);
		d3d.CmdList->SetDescriptorHeaps(1, &dxComp.mipHeap);

		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = textureResource.texture;

		D3D12_RESOURCE_BARRIER textureBarrier = {};
		textureBarrier.Transition.pResource = textureResource.texture;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		textureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		textureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		d3d.CmdList->ResourceBarrier(1, &textureBarrier);

		for (int mipLevel = 0; mipLevel < MaxMipLevels - 1; mipLevel++)
		{
			//Compute dest dimensions
			uint32_t dstWidth = Math::max(textureResource.textureInfo.width >> (mipLevel + 1), 1);
			uint32_t dstHeight = Math::max(textureResource.textureInfo.height >> (mipLevel + 1), 1);

			//Write new UAV to uav slot of mip heap
			//destTextureUAVDesc.Texture2D.MipSlice = 0;
			destTextureUAVDesc.Texture2D.MipSlice = static_cast<UINT>(mipLevel);
			d3d.Device->CreateUnorderedAccessView(textureResource.texture, nullptr, &destTextureUAVDesc, CPUhandle);
			CPUhandle.ptr += handleIncrement;

			float texelSizeX = 1.0f / dstWidth;
			float texelSizeY = 1.0f / dstHeight;
			float mip = static_cast<float>(mipLevel);

			//Set 32bit constants of texelsize of dest to use for texcoords 
			d3d.CmdList->SetComputeRoot32BitConstant(0, *reinterpret_cast<uint32_t*>(&texelSizeX), 0);
			d3d.CmdList->SetComputeRoot32BitConstant(0, *reinterpret_cast<uint32_t*>(&texelSizeY), 1);
			d3d.CmdList->SetComputeRoot32BitConstant(0, *reinterpret_cast<uint32_t*>(&mip), 2);

			//Set Descriptor tables on gpu side
			d3d.CmdList->SetComputeRootDescriptorTable(1, dxComp.mipHeap->GetGPUDescriptorHandleForHeapStart());
			GPUhandle.ptr += handleIncrement;
			d3d.CmdList->SetComputeRootDescriptorTable(2, GPUhandle);

			//Dispatch compute on dimensions of destination tex
			d3d.CmdList->Dispatch(Math::max(dstWidth / 8, 1u), Math::max(dstWidth / 8, 1u), 1);
			
			//barrier to wait on the texture to be freed up again
			d3d.CmdList->ResourceBarrier(1, &uavBarrier);
		}
			
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		textureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		d3d.CmdList->ResourceBarrier(1, &textureBarrier);

		D3D12::Submit_CmdList(d3d);
		D3D12::WaitForGPU(d3d);
		D3D12::Reset_CommandList(d3d);
	}

	/*
	* Create the vertex buffer.
	*/
	void Create_Vertex_Buffer(D3D12Global& d3d, D3D12Resources& resources, const SceneObject& sceneObj, uint32_t index)
	{
		const StaticMesh& model = sceneObj.Mesh;

		// Create the vertex buffer resource
		D3D12BufferCreateInfo info(((UINT)model.Vertices.size() * sizeof(Vertex)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		Create_Buffer(d3d, info, &resources.sceneObjResources[index].vertexBuffer);
#if NAME_D3D_RESOURCES
		resources.sceneObjResources[index].vertexBuffer->SetName(L"Vertex Buffer");
#endif

		// Copy the vertex data to the vertex buffer
		UINT8* pVertexDataBegin;
		D3D12_RANGE readRange = {};
		HRESULT hr = resources.sceneObjResources[index].vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		Utils::Validate(hr, L"Error: failed to map vertex buffer!");

		memcpy(pVertexDataBegin, model.Vertices.data(), info.size);
		resources.sceneObjResources[index].vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view
		resources.sceneObjResources[index].vertexBufferView.BufferLocation = resources.sceneObjResources[index].vertexBuffer->GetGPUVirtualAddress();
		resources.sceneObjResources[index].vertexBufferView.StrideInBytes = sizeof(Vertex);
		resources.sceneObjResources[index].vertexBufferView.SizeInBytes = static_cast<UINT>(info.size);
	}

	/**
	* Create the index buffer.
	*/
	void Create_Index_Buffer(D3D12Global& d3d, D3D12Resources& resources, const SceneObject& sceneObj, uint32_t index)
	{
		const StaticMesh& model = sceneObj.Mesh;

		// Create the index buffer resource
		D3D12BufferCreateInfo info((UINT)model.Indices.size() * sizeof(UINT), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		Create_Buffer(d3d, info, &resources.sceneObjResources[index].indexBuffer);
#if NAME_D3D_RESOURCES
		resources.sceneObjResources[index].indexBuffer->SetName(L"Index Buffer");
#endif

		// Copy the index data to the index buffer
		UINT8* pIndexDataBegin;
		D3D12_RANGE readRange = {};
		HRESULT hr = resources.sceneObjResources[index].indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
		Utils::Validate(hr, L"Error: failed to map index buffer!");

		memcpy(pIndexDataBegin, model.Indices.data(), info.size);
		resources.sceneObjResources[index].indexBuffer->Unmap(0, nullptr);

		// Initialize the index buffer view
		resources.sceneObjResources[index].indexBufferView.BufferLocation = resources.sceneObjResources[index].indexBuffer->GetGPUVirtualAddress();
		resources.sceneObjResources[index].indexBufferView.SizeInBytes = static_cast<UINT>(info.size);
		resources.sceneObjResources[index].indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	}

	/*
	* Create a constant buffer.
	*/
	void Create_Constant_Buffer(D3D12Global& d3d, ID3D12Resource** buffer, UINT64 size)
	{
		D3D12BufferCreateInfo bufferInfo((size + 255) & ~255, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		Create_Buffer(d3d, bufferInfo, buffer);
	}

	/**
	* Create the back buffer's RTV view.
	*/
	void Create_BackBuffer_RTV(D3D12Global& d3d, D3D12Resources& resources)
	{
		HRESULT hr;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

		rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();

		// Create a RTV for each back buffer
		for (UINT n = 0; n < 2; n++)
		{
			hr = d3d.SwapChain->GetBuffer(n, IID_PPV_ARGS(&d3d.BackBuffer[n]));
			Utils::Validate(hr, L"Error: failed to get swap chain buffer!");

			d3d.Device->CreateRenderTargetView(d3d.BackBuffer[n], nullptr, rtvHandle);

#if NAME_D3D_RESOURCES
			if (n == 0) d3d.BackBuffer[n]->SetName(L"Back Buffer 0");
			else d3d.BackBuffer[n]->SetName(L"Back Buffer 1");
#endif

			rtvHandle.ptr += resources.rtvDescSize;
		}
	}

	/**
	* Create and initialize the view constant buffer.
	*/
	void Create_View_CB(D3D12Global& d3d, D3D12Resources& resources)
	{
		Create_Constant_Buffer(d3d, &resources.viewCB, sizeof(ViewCB));
#if NAME_D3D_RESOURCES
		resources.viewCB->SetName(L"View Constant Buffer");
#endif

		HRESULT hr = resources.viewCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.viewCBStart));
		Utils::Validate(hr, L"Error: failed to map View constant buffer!");

		memcpy(resources.viewCBStart, &resources.viewCBData, sizeof(resources.viewCBData));
	}

	/**
	* Create and initialize the material constant buffer.
	*/
	void Create_Material_CB(D3D12Global& d3d, D3D12Resources& resources, const Material& material, uint32_t index)
	{
		MaterialCB& mat = resources.sceneObjResources[index].materialCBData;

		mat.resolution = DirectX::XMFLOAT4(material.TextureResolution.X, material.TextureResolution.Y, 0.f, 0.f);
		mat.hasDiffuse = !material.TexturePath.empty() ? 1u : 0u;
		//mat.hasDiffuse = 0u;
		mat.hasNormal = !material.NormalMapPath.empty() ? 1u : 0u;
		mat.hasTransparency = !material.OpacityMapPath.empty() ? 1u : 0u;

		mat.AmbientColor = Vector3fToDXFloat3(material.AmbientColor);
		mat.DiffuseColor = Vector3fToDXFloat3(material.DiffuseColor);
		mat.SpecularColor = Vector3fToDXFloat3(material.SpecularColor);
		mat.TransmitanceFilter = Vector3fToDXFloat3(material.TransmitanceFilter);
		mat.Shininess = material.Shininess;
		mat.RefractIndex = material.RefractIndex;

		//CORE_INFO("Transmittance filter = {0}, {1}, {2}", mat.TransmitanceFilter.x, mat.TransmitanceFilter.y, mat.TransmitanceFilter.z);

		Create_Constant_Buffer(d3d, &resources.sceneObjResources[index].materialCB, sizeof(MaterialCB));
//#if NAME_D3D_RESOURCES
//		resources.sceneObjResources[index].materialCB->SetName(L"Material Constant Buffer");
//#endif
		UINT8* pData;
		HRESULT hr = resources.sceneObjResources[index].materialCB->Map(0, nullptr, reinterpret_cast<void**>(&pData));
		Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

		memcpy(pData, &resources.sceneObjResources[index].materialCBData, sizeof(MaterialCB));

		//resources.sceneObjResources[index].materialCB->Unmap(0, nullptr);
	}

	void Create_Query_Heap(D3D12Global& d3d, D3D12Resources& resources)
	{
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Count = 10;
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

		HRESULT hr = d3d.Device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&resources.queryHeap));
		Utils::Validate(hr, L"Failed to create timestamp query heap!");
;	}

	/**
	* Create and initialize the trace parmeters constant buffer.
	*/
	void Create_Params_CB(D3D12Global& d3d, D3D12Resources& resources)
	{
		Create_Constant_Buffer(d3d, &resources.paramCB, sizeof(TracerParameters));
#if NAME_D3D_RESOURCES
		resources.paramCB->SetName(L"Trace Parameters Constant Buffer");
#endif
		HRESULT hr = resources.paramCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.paramCBStart));
		Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

		memcpy(resources.paramCBStart, &resources.paramCBData, sizeof(TracerParameters));
	}


	void Create_UIHeap(D3D12Global& d3d, D3D12Resources& resources)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.uiHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");
	}

	/**
	* Create the RTV descriptor heap.
	*/
	void Create_Descriptor_Heaps(D3D12Global& d3d, D3D12Resources& resources)
	{
		// Describe the RTV descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.NumDescriptors = 2;
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		// Create the RTV heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&resources.rtvHeap));
		Utils::Validate(hr, L"Error: failed to create RTV descriptor heap!");
#if NAME_D3D_RESOURCES
		resources.rtvHeap->SetName(L"RTV Descriptor Heap");
#endif

		resources.rtvDescSize = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	/**
	* Update the view constant buffer.
	*/
	void Update_View_CB(D3D12Global& d3d, D3D12Resources& resources, Camera& camera, Vector2f& jitterOffset, Vector2f& displayResolution)
	{
		DirectX::XMMATRIX view, invView;
		DirectX::XMFLOAT3 eye, focus, up;
		float aspect, fov;

		eye = DirectX::XMFLOAT3(camera.Position.X, camera.Position.Y, camera.Position.Z);
		up = DXMath::Vector3fToDXFloat3(camera.GetUpVector());

		aspect = (float)d3d.Width / (float)d3d.Height;
		fov = camera.FOV * (DirectX::XM_PI / 180.f);							// convert to radians

		focus = DXMath::DXFloat3AddFloat3(eye, DXMath::Vector3fToDXFloat3(camera.GetForward()));

		view = DirectX::XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&focus), XMLoadFloat3(&up));
		invView = XMMatrixInverse(NULL, view);

		resources.viewCBData.view = XMMatrixTranspose(invView);
		resources.viewCBData.viewOriginAndTanHalfFovY = DirectX::XMFLOAT4(eye.x, eye.y, eye.z, tanf(fov * 0.5f));
		resources.viewCBData.displayResolution = DirectX::XMFLOAT2(displayResolution.X, displayResolution.Y);

		resources.viewCBData.jitterOffset = DirectX::XMFLOAT2(jitterOffset.X, jitterOffset.Y);

		memcpy(resources.viewCBStart, &resources.viewCBData, sizeof(resources.viewCBData));
	}

	void Update_View_CB(D3D12Resources& resources, ViewCB& vcb)
	{
		resources.viewCBData = vcb;
		memcpy(resources.viewCBStart, &resources.viewCBData, sizeof(resources.viewCBData));
	}

	/**
	* Update the view constant buffer.
	*/
	void Update_Params_CB(D3D12Resources& resources, TracerParameters& params)
	{
		resources.paramCBData = params;
		memcpy(resources.paramCBStart, &resources.paramCBData, sizeof(resources.paramCBData));
	}

	void Update_SPP(D3D12Resources& resources, uint32_t newSpp)
	{
		resources.paramCBData.sqrtSamplesPerPixel = newSpp;
		memcpy(resources.paramCBStart, &resources.paramCBData, sizeof(resources.paramCBData));
	}

	/**
	 * Release the resources.
	 */
	void Destroy(D3D12Resources& resources)
	{
		if (resources.viewCB) resources.viewCB->Unmap(0, nullptr);
		if (resources.viewCBStart) resources.viewCBStart = nullptr;

		for(int i = 0; i < NUM_HISTORY_BUFFER; i++)
			SAFE_RELEASE(resources.DXROutput[i]);
		SAFE_RELEASE(resources.viewCB);
		SAFE_RELEASE(resources.rtvHeap);
		SAFE_RELEASE(resources.descriptorHeap);

		for (int i = 0; i < resources.sceneObjResources.size(); i++)
		{
			if (resources.sceneObjResources[i].materialCB) resources.sceneObjResources[i].materialCB->Unmap(0, nullptr);
			if (resources.sceneObjResources[i].materialCBStart) resources.sceneObjResources[i].materialCBStart = nullptr;
			SAFE_RELEASE(resources.sceneObjResources[i].vertexBuffer);
			SAFE_RELEASE(resources.sceneObjResources[i].indexBuffer);
			SAFE_RELEASE(resources.sceneObjResources[i].materialCB);
			//SAFE_RELEASE(resources.sceneObjResources[i].diffuseTex.texture);
			//SAFE_RELEASE(resources.sceneObjResources[i].diffuseTex.textureUploadResource);			
			//SAFE_RELEASE(resources.sceneObjResources[i].normalTex.texture);
			//SAFE_RELEASE(resources.sceneObjResources[i].normalTex.textureUploadResource);

			//Iterate texures in resources and release each texture and texture upload
		}
	}

	void Create_ReadBackResources(D3D12Global& d3d, D3D12Resources& resources)
	{
		D3D12_RESOURCE_DESC readbackBufferDesc{};
		readbackBufferDesc.DepthOrArraySize = 1;
		readbackBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readbackBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		readbackBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		readbackBufferDesc.Width = d3d.Width * d3d.Height * 4;
		readbackBufferDesc.Height = 1;
		readbackBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		readbackBufferDesc.MipLevels = 1;
		readbackBufferDesc.SampleDesc.Count = 1;
		readbackBufferDesc.SampleDesc.Quality = 0;


		HRESULT hr = d3d.Device->CreateCommittedResource(
			&ReadBackProperties, D3D12_HEAP_FLAG_NONE, &readbackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.OutputReadBack));
		Utils::Validate(hr, L"Failed to create output readback buffer");

		readbackBufferDesc.Width = 10 * 8;

		hr = d3d.Device->CreateCommittedResource(
			&ReadBackProperties, D3D12_HEAP_FLAG_NONE, &readbackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.TimestampReadBack));
		Utils::Validate(hr, L"Failed to create timestamp readback buffer");
	}
}

namespace DXR
{

	/**
	* Create the bottom level acceleration structure.
	*/
	void Create_Bottom_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, Scene& scene)
	{

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;

		for (int i = 0; i < scene.SceneObjects.size(); i++)
		{
			StaticMesh& model = scene.SceneObjects[i].Mesh;

			// Describe the geometry that goes in the bottom acceleration structure(s)
			D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
			geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geometryDesc.Triangles.VertexBuffer.StartAddress = resources.sceneObjResources[i].vertexBuffer->GetGPUVirtualAddress();
			geometryDesc.Triangles.VertexBuffer.StrideInBytes = resources.sceneObjResources[i].vertexBufferView.StrideInBytes;
			geometryDesc.Triangles.VertexCount = static_cast<UINT>(model.Vertices.size());
			geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geometryDesc.Triangles.IndexBuffer = resources.sceneObjResources[i].indexBuffer->GetGPUVirtualAddress();
			geometryDesc.Triangles.IndexFormat = resources.sceneObjResources[i].indexBufferView.Format;
			geometryDesc.Triangles.IndexCount = static_cast<UINT>(model.Indices.size());
			geometryDesc.Triangles.Transform3x4 = 0;
			geometryDesc.Flags = !scene.SceneObjects[i].Mesh.HasTransparency ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

			geometryDescs.push_back(geometryDesc);
		}
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;

		// Get the size requirements for the BLAS buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
		ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		ASInputs.pGeometryDescs = geometryDescs.data();
		ASInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
		ASInputs.Flags = buildFlags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
		d3d.Device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

		ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
		ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

		// Create the BLAS scratch buffer
		D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		bufferInfo.alignment = Math::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.BLAS.pScratch);

#if NAME_D3D_RESOURCES
		dxr.BLAS.pScratch->SetName(L"DXR BLAS Scratch");
#endif

		// Create the BLAS buffer
		bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
		bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.BLAS.pResult);
#if NAME_D3D_RESOURCES
		dxr.BLAS.pResult->SetName(L"DXR BLAS");
#endif

		// Describe and build the bottom level acceleration structure
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs = ASInputs;
		buildDesc.ScratchAccelerationStructureData = dxr.BLAS.pScratch->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = dxr.BLAS.pResult->GetGPUVirtualAddress();

		d3d.CmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
		

		// Wait for the BLAS build to complete
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = dxr.BLAS.pResult;
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		d3d.CmdList->ResourceBarrier(1, &uavBarrier);

		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.BLAS.pCompactResult);
		d3d.CmdList->CopyRaytracingAccelerationStructure(dxr.BLAS.pCompactResult->GetGPUVirtualAddress(), dxr.BLAS.pResult->GetGPUVirtualAddress(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);

		d3d.CmdList->ResourceBarrier(1, &uavBarrier);
	}

	/**
	* Create the top level acceleration structure and its associated buffers.
	*/
	void Create_Top_Level_AS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources)
	{
		// Describe the TLAS geometry instance(s)
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.InstanceMask = 0xFF;
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.AccelerationStructure = dxr.BLAS.pCompactResult->GetGPUVirtualAddress();
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

		// Create the TLAS instance buffer
		D3D12BufferCreateInfo instanceBufferInfo;
		instanceBufferInfo.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
		instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
		instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
		D3DResources::Create_Buffer(d3d, instanceBufferInfo, &dxr.TLAS.pInstanceDesc);
#if NAME_D3D_RESOURCES
		dxr.TLAS.pInstanceDesc->SetName(L"DXR TLAS Instance Descriptors");
#endif

		// Copy the instance data to the buffer
		UINT8* pData;
		dxr.TLAS.pInstanceDesc->Map(0, nullptr, (void**)&pData);
		memcpy(pData, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		dxr.TLAS.pInstanceDesc->Unmap(0, nullptr);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// Get the size requirements for the TLAS buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
		ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		ASInputs.InstanceDescs = dxr.TLAS.pInstanceDesc->GetGPUVirtualAddress();
		ASInputs.NumDescs = 1;
		ASInputs.Flags = buildFlags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
		d3d.Device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

		ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
		ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

		// Set TLAS size
		dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

		// Create TLAS scratch buffer
		D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		bufferInfo.alignment = Math::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.TLAS.pScratch);
#if NAME_D3D_RESOURCES
		dxr.TLAS.pScratch->SetName(L"DXR TLAS Scratch");
#endif

		// Create the TLAS buffer
		bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
		bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.TLAS.pResult);
#if NAME_D3D_RESOURCES
		dxr.TLAS.pResult->SetName(L"DXR TLAS");
#endif

		// Describe and build the TLAS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs = ASInputs;
		buildDesc.ScratchAccelerationStructureData = dxr.TLAS.pScratch->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = dxr.TLAS.pResult->GetGPUVirtualAddress();

		d3d.CmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		// Wait for the TLAS build to complete
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = dxr.TLAS.pResult;
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		d3d.CmdList->ResourceBarrier(1, &uavBarrier);
	}

	/**
	* Load and create the DXR Ray Generation program and root signature.
	*/
	void Create_RayGen_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the ray generation shader
		dxr.rgs = RtProgram(D3D12ShaderInfo(L"Shaders\\RayGen.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.rgs);

		dxr.rgsCentral = RtProgram(D3D12ShaderInfo(L"Shaders\\RayGenCentral.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.rgsCentral);

		// Describe the ray generation root signature
		D3D12_DESCRIPTOR_RANGE ranges[3];

		ranges[0].BaseShaderRegister = 0;
		ranges[0].NumDescriptors = 3;
		ranges[0].RegisterSpace = 0;
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[0].OffsetInDescriptorsFromTableStart = 0;

		ranges[1].BaseShaderRegister = 0;
		ranges[1].NumDescriptors = 4 + NUM_HISTORY_BUFFER;
		ranges[1].RegisterSpace = 0;
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[1].OffsetInDescriptorsFromTableStart = ranges[0].NumDescriptors;

		ranges[2].BaseShaderRegister = 0;
		ranges[2].NumDescriptors = 7;
		ranges[2].RegisterSpace = 0;
		ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[2].OffsetInDescriptorsFromTableStart = ranges[0].NumDescriptors + ranges[1].NumDescriptors;

		D3D12_ROOT_PARAMETER param0 = {};
		param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
		param0.DescriptorTable.pDescriptorRanges = ranges;

		D3D12_ROOT_PARAMETER rootParams[1] = { param0 };

		dxr.rayTextureSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		dxr.rayTextureSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		dxr.rayTextureSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		dxr.rayTextureSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		dxr.rayTextureSampler.MipLODBias = 0.0f;
		dxr.rayTextureSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		dxr.rayTextureSampler.MinLOD = 0.0f;
		dxr.rayTextureSampler.MaxLOD = D3D12_FLOAT32_MAX;
		dxr.rayTextureSampler.MaxAnisotropy = 0;
		dxr.rayTextureSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		dxr.rayTextureSampler.ShaderRegister = 0;
		dxr.rayTextureSampler.RegisterSpace = 0;
		dxr.rayTextureSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.NumParameters = _countof(rootParams);
		rootDesc.pParameters = rootParams;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		rootDesc.NumStaticSamplers = 1;
		rootDesc.pStaticSamplers = &dxr.rayTextureSampler;

		// Create the root signature
		dxr.rgs.pRootSignature = D3D12::Create_Root_Signature(d3d, rootDesc);
		dxr.rgsCentral.pRootSignature = D3D12::Create_Root_Signature(d3d, rootDesc);
#if NAME_D3D_RESOURCES
		dxr.rgs.pRootSignature->SetName(L"DXR RGS Root Signature");
#endif
	}

	/**
	* Load and create the DXR Miss program and root signature.
	*/
	void Create_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the miss shader
		dxr.miss = RtProgram(D3D12ShaderInfo(L"Shaders\\Miss.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.miss);
	}	

	/**
	* Load and create the DXR Shadow Miss program and root signature.
	*/
	void Create_Shadow_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the shadow miss shader
		dxr.shadow_miss = RtProgram(D3D12ShaderInfo(L"Shaders\\ShadowMiss.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.shadow_miss);
	}

	/**
	* Load and create the DXR Closest Hit program and root signature.
	*/
	void Create_Closest_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the Closest Hit shader
		dxr.hit = HitProgram(L"Hit");
		dxr.hit.chs = RtProgram(D3D12ShaderInfo(L"Shaders\\ClosestHit.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.hit.chs);
	}

	/**
	* Load and create the DXR Closest Hit program and root signature.
	*/
	void Add_Alpha_AnyHit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the Closest Hit shader
		dxr.hit.ahs = RtProgram(D3D12ShaderInfo(L"Shaders\\AlphaAnyHit.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.hit.ahs);
	}

	void Add_Shadow_AnyHit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the Closest Hit shader
		dxr.shadow_hit.ahs = RtProgram(D3D12ShaderInfo(L"Shaders\\ShadowAnyHit.hlsl", L"ShadowAnyHit", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.shadow_hit.ahs);
	}

	/**
	* Load and create the DXR Shadow Hit program and root signature.
	*/
	void Create_Shadow_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler)
	{
		// Load and compile the Shadow Hit shader
		dxr.shadow_hit = HitProgram(L"ShadowHit");
		dxr.shadow_hit.chs = RtProgram(D3D12ShaderInfo(L"Shaders\\ShadowHit.hlsl", L"", L"lib_6_3"));
		D3DShaders::Compile_Shader(shaderCompiler, dxr.shadow_hit.chs);
	}

	/**
	* Create the DXR pipeline state object.
	*/
	void Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr)
	{
		UINT index = 0;
		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		subobjects.resize(16);

		// Add state subobject for the Shadow Miss shader
		D3D12_EXPORT_DESC smsExportDesc = {};
		smsExportDesc.Name = L"Shadow_Miss_5";
		smsExportDesc.ExportToRename = L"ShadowMiss";
		smsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	smsLibDesc = {};
		smsLibDesc.DXILLibrary.BytecodeLength = dxr.shadow_miss.blob->GetBufferSize();
		smsLibDesc.DXILLibrary.pShaderBytecode = dxr.shadow_miss.blob->GetBufferPointer();
		smsLibDesc.NumExports = 1;
		smsLibDesc.pExports = &smsExportDesc;

		D3D12_STATE_SUBOBJECT sms = {};
		sms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		sms.pDesc = &smsLibDesc;

		subobjects[index++] = sms;

		// Add state subobject for the Closest Hit shader
		D3D12_EXPORT_DESC shsExportDesc = {};
		shsExportDesc.Name = L"ShadowHit_76";
		shsExportDesc.ExportToRename = L"ShadowHit";
		shsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	shsLibDesc = {};
		shsLibDesc.DXILLibrary.BytecodeLength = dxr.shadow_hit.chs.blob->GetBufferSize();
		shsLibDesc.DXILLibrary.pShaderBytecode = dxr.shadow_hit.chs.blob->GetBufferPointer();
		shsLibDesc.NumExports = 1;
		shsLibDesc.pExports = &shsExportDesc;

		D3D12_STATE_SUBOBJECT shs = {};
		shs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		shs.pDesc = &shsLibDesc;

		subobjects[index++] = shs;

		// Add state subobject for the Shadow Any Hit shader
		D3D12_EXPORT_DESC sahsExportDesc = {};
		sahsExportDesc.Name = L"ShadowAny";
		sahsExportDesc.ExportToRename = L"ShadowAnyHit";
		sahsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	sahsLibDesc = {};
		sahsLibDesc.DXILLibrary.BytecodeLength = dxr.shadow_hit.ahs.blob->GetBufferSize();
		sahsLibDesc.DXILLibrary.pShaderBytecode = dxr.shadow_hit.ahs.blob->GetBufferPointer();
		sahsLibDesc.NumExports = 1;
		sahsLibDesc.pExports = &sahsExportDesc;

		D3D12_STATE_SUBOBJECT sahs = {};
		sahs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		sahs.pDesc = &sahsLibDesc;

		subobjects[index++] = sahs;

		// Add a state subobject for the hit group
		D3D12_HIT_GROUP_DESC shadowHitGroupDesc = {};
		shadowHitGroupDesc.ClosestHitShaderImport = L"ShadowHit_76";
		shadowHitGroupDesc.AnyHitShaderImport = L"ShadowAny";
		shadowHitGroupDesc.HitGroupExport = L"ShadowHitGroup";

		D3D12_STATE_SUBOBJECT shadowHitGroup = {};
		shadowHitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		shadowHitGroup.pDesc = &shadowHitGroupDesc;

		subobjects[index++] = shadowHitGroup;

		// Add state subobject for the RGS
		D3D12_EXPORT_DESC rgsExportDesc = {};
		rgsExportDesc.Name = L"RayGen_12";
		rgsExportDesc.ExportToRename = L"RayGen";
		rgsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	rgsLibDesc = {};
		rgsLibDesc.DXILLibrary.BytecodeLength = dxr.rgs.blob->GetBufferSize();
		rgsLibDesc.DXILLibrary.pShaderBytecode = dxr.rgs.blob->GetBufferPointer();
		rgsLibDesc.NumExports = 1;
		rgsLibDesc.pExports = &rgsExportDesc;

		D3D12_STATE_SUBOBJECT rgs = {};
		rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		rgs.pDesc = &rgsLibDesc;

		subobjects[index++] = rgs;

		// Add state subobject for the RGS
		D3D12_EXPORT_DESC rgsCentralExportDesc = {};
		rgsCentralExportDesc.Name = L"RayGen_Central";
		rgsCentralExportDesc.ExportToRename = L"RayGenCentral";
		rgsCentralExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	rgsCentralLibDesc = {};
		rgsCentralLibDesc.DXILLibrary.BytecodeLength = dxr.rgsCentral.blob->GetBufferSize();
		rgsCentralLibDesc.DXILLibrary.pShaderBytecode = dxr.rgsCentral.blob->GetBufferPointer();
		rgsCentralLibDesc.NumExports = 1;
		rgsCentralLibDesc.pExports = &rgsCentralExportDesc;

		D3D12_STATE_SUBOBJECT rgsCentral = {};
		rgsCentral.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		rgsCentral.pDesc = &rgsCentralLibDesc;

		subobjects[index++] = rgsCentral;

		// Add state subobject for the Miss shader
		D3D12_EXPORT_DESC msExportDesc = {};
		msExportDesc.Name = L"Miss_5";
		msExportDesc.ExportToRename = L"Miss";
		msExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
		msLibDesc.DXILLibrary.BytecodeLength = dxr.miss.blob->GetBufferSize();
		msLibDesc.DXILLibrary.pShaderBytecode = dxr.miss.blob->GetBufferPointer();
		msLibDesc.NumExports = 1;
		msLibDesc.pExports = &msExportDesc;

		D3D12_STATE_SUBOBJECT ms = {};
		ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		ms.pDesc = &msLibDesc;

		subobjects[index++] = ms;

		// Add state subobject for the Closest Hit shader
		D3D12_EXPORT_DESC chsExportDesc = {};
		chsExportDesc.Name = L"ClosestHit_76";
		chsExportDesc.ExportToRename = L"ClosestHit";
		chsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
		chsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.chs.blob->GetBufferSize();
		chsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.chs.blob->GetBufferPointer();
		chsLibDesc.NumExports = 1;
		chsLibDesc.pExports = &chsExportDesc;

		D3D12_STATE_SUBOBJECT chs = {};
		chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		chs.pDesc = &chsLibDesc;

		subobjects[index++] = chs;

		// Add state subobject for the Alpha Any Hit shader
		D3D12_EXPORT_DESC ahsExportDesc = {};
		ahsExportDesc.Name = L"AlphaHit";
		ahsExportDesc.ExportToRename = L"AlphaAnyHit";
		ahsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		D3D12_DXIL_LIBRARY_DESC	ahsLibDesc = {};
		ahsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.ahs.blob->GetBufferSize();
		ahsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.ahs.blob->GetBufferPointer();
		ahsLibDesc.NumExports = 1;
		ahsLibDesc.pExports = &ahsExportDesc;

		D3D12_STATE_SUBOBJECT ahs = {};
		ahs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		ahs.pDesc = &ahsLibDesc;

		subobjects[index++] = ahs;

		// Add a state subobject for the hit group
		D3D12_HIT_GROUP_DESC hitGroupDesc = {};
		hitGroupDesc.ClosestHitShaderImport = L"ClosestHit_76";
		hitGroupDesc.AnyHitShaderImport = L"AlphaHit";
		hitGroupDesc.HitGroupExport = L"HitGroup";

		D3D12_STATE_SUBOBJECT hitGroup = {};
		hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroup.pDesc = &hitGroupDesc;

		subobjects[index++] = hitGroup;

		// Add a state subobject for the shader payload configuration
		D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
		shaderDesc.MaxPayloadSizeInBytes = sizeof(DirectX::XMFLOAT4) + 1 * 5 * sizeof(float) + sizeof(uint32_t);
		shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

		D3D12_STATE_SUBOBJECT shaderConfigObject = {};
		shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		shaderConfigObject.pDesc = &shaderDesc;

		subobjects[index++] = shaderConfigObject;

		// Create a list of the shader export names that use the payload
		const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup", L"ShadowHitGroup", L"Shadow_Miss_5", L"RayGen_Central"};

		// Add a state subobject for the association between shaders and the payload
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
		shaderPayloadAssociation.NumExports = _countof(shaderExports);
		shaderPayloadAssociation.pExports = shaderExports;
		shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

		D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
		shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

		subobjects[index++] = shaderPayloadAssociationObject;

		// Add a state subobject for the shared root signature 
		D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
		rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		rayGenRootSigObject.pDesc = &dxr.rgs.pRootSignature;

		subobjects[index++] = rayGenRootSigObject;

		// Create a list of the shader export names that use the root signature
		const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5", L"Shadow_Miss_5", L"ShadowHitGroup", L"RayGen_Central"};

		// Add a state subobject for the association between the RayGen shader and the local root signature
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
		rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
		rayGenShaderRootSigAssociation.pExports = rootSigExports;
		rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

		D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
		rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;

		subobjects[index++] = rayGenShaderRootSigAssociationObject;

		D3D12_STATE_SUBOBJECT globalRootSig;
		globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		globalRootSig.pDesc = &dxr.miss.pRootSignature;

		subobjects[index++] = globalRootSig;

		// Add a state subobject for the ray tracing pipeline config
		D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
		pipelineConfig.MaxTraceRecursionDepth = 6;

		D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
		pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		pipelineConfigObject.pDesc = &pipelineConfig;

		subobjects[index++] = pipelineConfigObject;

		// Describe the Ray Tracing Pipeline State Object
		D3D12_STATE_OBJECT_DESC pipelineDesc = {};
		pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
		pipelineDesc.pSubobjects = subobjects.data();

		// Create the RT Pipeline State Object (RTPSO)
		HRESULT hr = d3d.Device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&dxr.rtpso));
		Utils::Validate(hr, L"Error: failed to create state object!");
#if NAME_D3D_RESOURCES
		dxr.rtpso->SetName(L"DXR Pipeline State Object");
#endif

		// Get the RTPSO properties
		hr = dxr.rtpso->QueryInterface(IID_PPV_ARGS(&dxr.rtpsoInfo));
		Utils::Validate(hr, L"Error: failed to get RTPSO info object!");
	}

	/**
	* Create the DXR shader table.
	*/
	void Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources)
	{
		/*
		The Shader Table layout is as follows:
			Entry 0 - Ray Generation shader
			Entry 1 - Miss shader
			Ëntry 2 - Shadow miss
			Entry 3 - Shadow hit
			NSceneObjs ClosestHit entries
		All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
		The ray generation program requires the largest entry:
			32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
		  +  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
		  = 40 bytes ->> aligns to 64 bytes
		The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
		*/

		uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		uint32_t shaderTableSize = 0;

		dxr.shaderTableRecordSize = shaderIdSize;
		dxr.shaderTableRecordSize += 8;							// CBV/SRV/UAV descriptor table
		dxr.shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.shaderTableRecordSize);

		shaderTableSize = (dxr.shaderTableRecordSize * (5 + static_cast<uint32_t>(resources.sceneObjResources.size())));		// 5 shader records in the table
		shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);

		// Create the shader table buffer
		D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.shaderTable);
#if NAME_D3D_RESOURCES
		dxr.shaderTable->SetName(L"DXR Shader Table");
#endif

		// Map the buffer
		uint8_t* pData;
		HRESULT hr = dxr.shaderTable->Map(0, nullptr, (void**)&pData);
		Utils::Validate(hr, L"Error: failed to map shader table!");

		//GEN

		// Gen Shader Record 0 - Ray Generation program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
		memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

		// Set the root parameter data. Point to start of descriptor heap.
		*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

		// Gen Shader Record 1 - Ray Generation program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
		pData += dxr.shaderTableRecordSize;
		memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"RayGen_Central"), shaderIdSize);

		// Set the root parameter data. Point to start of descriptor heap.
		*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

		//MISS

		// Shader Miss Record 0 - Shadow Miss program (no local root arguments to set)
		pData += dxr.shaderTableRecordSize;
		memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Shadow_Miss_5"), shaderIdSize);

		// Shader Miss Record 1 - Miss program (no local root arguments to set)
		pData += dxr.shaderTableRecordSize;
		memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);


		//HIT

		// Shader HitGroup Record 0 - Shadow Hit program (no local root arguments to set)
		pData += dxr.shaderTableRecordSize;
		memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"ShadowHitGroup"), shaderIdSize);


		//// Set the root parameter data. Point to start of descriptor heap.
		//*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.sceneObjResources[0].descriptorHeap->GetGPUDescriptorHandleForHeapStart();

		UINT handleIncrement = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * DX12Constants::descriptors_per_shader;
		D3D12_GPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		for (int i = 0; i < resources.sceneObjResources.size(); i++)
		{
			// Shader HitGroup Record 1 - Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
			pData += dxr.shaderTableRecordSize;
			memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

			*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = handle;
			handle.ptr += handleIncrement;
		}

		// Unmap
		dxr.shaderTable->Unmap(0, nullptr);
	}

	/*
	* Descriptor heap for clearing the framebuffer
	*/
	//void Create_Non_Shader_Visible_Heap(D3D12Global& d3d, D3D12Resources& resources)
	//{
	//	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	//	desc.NumDescriptors = 1;
	//	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	//	// Create the descriptor heap
	//	HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.cpuOnlyHeap));
	//	Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

	//	// Get the descriptor heap handle and increment size
	//	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cpuOnlyHeap->GetCPUDescriptorHandleForHeapStart();

	//	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	//	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	//	d3d.Device->CreateUnorderedAccessView(resources.DXROutput[1], nullptr, &uavDesc, handle);
	//}     

	/**
	* Create the DXR descriptor heap for CBVs, SRVs, and the output UAV.
	*/
	void Create_Descriptor_Heaps(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, Scene& scene)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = DX12Constants::descriptors_per_shader * static_cast<UINT>(resources.sceneObjResources.size());
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		HRESULT hr = d3d.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.descriptorHeap));
		Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

		// Get the descriptor heap handle and increment size
		D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		UINT handleIncrement = d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#if NAME_D3D_RESOURCES
		resources.descriptorHeap->SetName(L"DXR Descriptor Heap");
#endif

		for (int i = 0; i < scene.SceneObjects.size(); i++)
		{
			// Describe the CBV/SRV/UAV heap
			// Need 8 entries per scene object:
			// 1 CBV for the ViewCB
			// 1 CBV for the MaterialCB
			// 1 CBV for the Tracer ParamsCB
			// 1 UAV for the RT output
			// 1 SRV for the Scene BVH
			// 1 SRV for the index buffer
			// 1 SRV for the vertex buffer
			// 1 SRV for the diffuse texture
			// 1 SRV for the normal map
			// 1 SRV for the opacity map

			// Create the ViewCB CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.viewCBData));
			cbvDesc.BufferLocation = resources.viewCB->GetGPUVirtualAddress();

			d3d.Device->CreateConstantBufferView(&cbvDesc, handle);
			handle.ptr += handleIncrement;

			// Create the MaterialCB CBV
			cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.sceneObjResources[i].materialCBData));
			cbvDesc.BufferLocation = resources.sceneObjResources[i].materialCB->GetGPUVirtualAddress();

			d3d.Device->CreateConstantBufferView(&cbvDesc, handle);
			handle.ptr += handleIncrement;

			//Create the Tracer ParamsCB CBV
			cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.paramCBData));
			cbvDesc.BufferLocation = resources.paramCB->GetGPUVirtualAddress();

			d3d.Device->CreateConstantBufferView(&cbvDesc, handle);
			handle.ptr += handleIncrement;

			// Create the DXR output buffer UAV
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			for (int j = 0; j < NUM_HISTORY_BUFFER; j++)
			{
				d3d.Device->CreateUnorderedAccessView(resources.DXROutput[j], nullptr, &uavDesc, handle);
				handle.ptr += handleIncrement;
			}

			// Create the Motion output buffer UAV
			d3d.Device->CreateUnorderedAccessView(resources.MotionOutput[0], nullptr, &uavDesc, handle);
			handle.ptr += handleIncrement;

			d3d.Device->CreateUnorderedAccessView(resources.MotionOutput[1], nullptr, &uavDesc, handle);
			handle.ptr += handleIncrement;

			// Create the WorldPos buffer UAV
			d3d.Device->CreateUnorderedAccessView(resources.WorldPosBuffer[0], nullptr, &uavDesc, handle);
			handle.ptr += handleIncrement;

			d3d.Device->CreateUnorderedAccessView(resources.WorldPosBuffer[1], nullptr, &uavDesc, handle);
			handle.ptr += handleIncrement;

			// Create the DXR Top Level Acceleration Structure SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.RaytracingAccelerationStructure.Location = dxr.TLAS.pResult->GetGPUVirtualAddress();

			d3d.Device->CreateShaderResourceView(nullptr, &srvDesc, handle);
			handle.ptr += handleIncrement;

			// Create the index buffer SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
			indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			indexSRVDesc.Buffer.StructureByteStride = 0;
			indexSRVDesc.Buffer.FirstElement = 0;
			indexSRVDesc.Buffer.NumElements = (static_cast<UINT>(scene.SceneObjects[i].Mesh.Indices.size()) * sizeof(UINT)) / sizeof(float);
			indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(resources.sceneObjResources[i].indexBuffer, &indexSRVDesc, handle);
			handle.ptr += handleIncrement;

			// Create the vertex buffer SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
			vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			vertexSRVDesc.Buffer.StructureByteStride = 0;
			vertexSRVDesc.Buffer.FirstElement = 0;
			vertexSRVDesc.Buffer.NumElements = (static_cast<UINT>(scene.SceneObjects[i].Mesh.Vertices.size()) * sizeof(Vertex)) / sizeof(float);
			vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(resources.sceneObjResources[i].vertexBuffer, &vertexSRVDesc, handle);
			handle.ptr += handleIncrement;


			auto& diffuseTex = resources.Textures[resources.sceneObjResources[i].diffuseTexKey];

			// Create the material texture SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc = {};
			textureSRVDesc.Format = diffuseTex.resourceDesc.Format != DXGI_FORMAT_UNKNOWN ? diffuseTex.resourceDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
			textureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			textureSRVDesc.Texture2D.MipLevels = diffuseTex.resourceDesc.MipLevels;
			textureSRVDesc.Texture2D.MostDetailedMip = 0;
			textureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(diffuseTex.texture, &textureSRVDesc, handle);
			handle.ptr += handleIncrement;


			auto& normalTex = resources.Textures[resources.sceneObjResources[i].normalTexKey];

			// Create the normals texture SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC normalsSRVDesc = {};
			normalsSRVDesc.Format = normalTex.resourceDesc.Format != DXGI_FORMAT_UNKNOWN ? normalTex.resourceDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
			normalsSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			normalsSRVDesc.Texture2D.MipLevels = normalTex.resourceDesc.MipLevels;
			normalsSRVDesc.Texture2D.MostDetailedMip = 0;
			normalsSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(normalTex.texture, &normalsSRVDesc, handle);
			handle.ptr += handleIncrement;


			auto& opacityTex = resources.Textures[resources.sceneObjResources[i].opacityTexKey];

			// Create the opacity map SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC opacitySRVDesc = {};
			opacitySRVDesc.Format = opacityTex.resourceDesc.Format != DXGI_FORMAT_UNKNOWN ? opacityTex.resourceDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
			opacitySRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			opacitySRVDesc.Texture2D.MipLevels = opacityTex.resourceDesc.MipLevels;
			opacitySRVDesc.Texture2D.MostDetailedMip = 0;
			opacitySRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(opacityTex.texture, &opacitySRVDesc, handle);
			handle.ptr += handleIncrement;


			auto& blueNoiseTex = resources.Textures[Utils::GetResourcePath(DX12Constants::blue_noise_tex_path)];

			// Create the blueNoise map SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC blueNoiseSRVDesc = {};
			blueNoiseSRVDesc.Format = blueNoiseTex.resourceDesc.Format != DXGI_FORMAT_UNKNOWN ? blueNoiseTex.resourceDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
			blueNoiseSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			blueNoiseSRVDesc.Texture2D.MipLevels = 1;
			blueNoiseSRVDesc.Texture2D.MostDetailedMip = 0;
			blueNoiseSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			d3d.Device->CreateShaderResourceView(blueNoiseTex.texture, &blueNoiseSRVDesc, handle);
			handle.ptr += handleIncrement;
		}
	}

	void Create_DLSS_Output(D3D12Global& d3d, D3D12Resources& resources)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = d3d.Width;
		desc.Height = d3d.Height;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		// Create the buffer resource
		HRESULT hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&resources.DLSSOutput));
		Utils::Validate(hr, L"Error: failed to create DLSS output buffer!");

		CORE_INFO("DLSS Output created with dimensions {0}x{1}", desc.Width, desc.Height);
	}

	/**
	* Create the DXR output buffer.
	*/
	void Create_DXR_Output(D3D12Global& d3d, D3D12Resources& resources)
	{
		// Describe the DXR output resource (texture)
		// Dimensions and format should match the swapchain
		// Initialize as a copy source, since we will copy this buffer's contents to the swapchain
		D3D12_RESOURCE_DESC desc = {};
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = d3d.Width;
		desc.Height = d3d.Height;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		HRESULT hr;
		for (int i = 0; i < NUM_HISTORY_BUFFER; i++)
		{
			// Create the buffer resource
			hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.DXROutput[i]));
			Utils::Validate(hr, L"Error: failed to create DXR output buffer!");
		}

//#if NAME_D3D_RESOURCES
//		resources.DXROutput->SetName(L"DXR Output Buffer");
//#endif
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.MotionOutput[0]));
		Utils::Validate(hr, L"Error: failed to create Motion output buffer!");

		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.MotionOutput[1]));
		Utils::Validate(hr, L"Error: failed to create Motion output buffer!");

		//desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.WorldPosBuffer[0]));
		Utils::Validate(hr, L"Error: failed to create WorldPos buffer!");

		hr = d3d.Device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.WorldPosBuffer[1]));
		Utils::Validate(hr, L"Error: failed to create WorldPos buffer!");
	}

	/**
	* Builds the frame's DXR command list.
	*/
	void Build_Command_List(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, D3D12Compute& dxComp, DLSSConfig& dlssConfig, bool scrshotRequested, bool dlssPreScrshot, bool clearTAA, bool TAAEnabled)
	{
		D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};
		D3D12_RESOURCE_BARRIER CounterBarriers[2] = {};

		// Transition the back buffer to a copy destination
		OutputBarriers[0].Transition.pResource = d3d.BackBuffer[d3d.FrameIndex];
		OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Transition the DXR output buffer to a copy source 
		OutputBarriers[1].Transition.pResource = resources.DLSSOutput;
		OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		OutputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		D3D12_RESOURCE_BARRIER uavBarriers[5 + NUM_HISTORY_BUFFER] = {};
		for (int i = 0; i < _countof(uavBarriers); i++)
			uavBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

		for (int i = 0; i < NUM_HISTORY_BUFFER; i++)
			uavBarriers[i].UAV.pResource = resources.DXROutput[i];

		uavBarriers[NUM_HISTORY_BUFFER + 0].UAV.pResource = resources.MotionOutput[0];
		uavBarriers[NUM_HISTORY_BUFFER + 1].UAV.pResource = resources.MotionOutput[1];
		uavBarriers[NUM_HISTORY_BUFFER + 2].UAV.pResource = resources.WorldPosBuffer[0];
		uavBarriers[NUM_HISTORY_BUFFER + 3].UAV.pResource = resources.WorldPosBuffer[1];
		uavBarriers[NUM_HISTORY_BUFFER + 4].UAV.pResource = resources.DLSSOutput;

		D3D12_RESOURCE_BARRIER dlssUAVBarriers[3] = {};
		dlssUAVBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		dlssUAVBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		dlssUAVBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

		dlssUAVBarriers[0].UAV.pResource = resources.Log2CartOutput;
		dlssUAVBarriers[1].UAV.pResource = resources.DLSSDepthInput;
		dlssUAVBarriers[2].UAV.pResource = resources.FinalMotionOutput;


		// Wait for the transitions to complete
		d3d.CmdList->ResourceBarrier(2, OutputBarriers);

		ID3D12DescriptorHeap* ppHeaps[1] = {resources.descriptorHeap};
		d3d.CmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Dispatch rays
		D3D12_DISPATCH_RAYS_DESC desc = {};
		desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress();
		desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

		desc.MissShaderTable.StartAddress = desc.RayGenerationShaderRecord.StartAddress + desc.RayGenerationShaderRecord.SizeInBytes * 2;
		desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize * 2;		// Only a single Miss program entry
		desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

		desc.HitGroupTable.StartAddress = desc.MissShaderTable.StartAddress + desc.MissShaderTable.SizeInBytes;
		desc.HitGroupTable.SizeInBytes = dxr.shaderTableRecordSize * (1 + resources.sceneObjResources.size());			// Only a single Hit program entry
		desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

		desc.Width = d3d.Width;
		desc.Height = d3d.Height;
		desc.Depth = 1;

		d3d.CmdList->SetPipelineState1(dxr.rtpso);

		//Start raytracing time
		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0);

		d3d.CmdList->DispatchRays(&desc);
		d3d.CmdList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
		
		desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + dxr.shaderTableRecordSize;
		desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

		d3d.CmdList->DispatchRays(&desc);
		d3d.CmdList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

		//End raytracing time
		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 1);


		//Compute
		d3d.CmdList->SetDescriptorHeaps(1, &dxComp.descriptorHeap);
		d3d.CmdList->SetPipelineState(dxComp.cps);
		d3d.CmdList->SetComputeRootSignature(dxComp.pRootSignature);

		d3d.CmdList->SetComputeRootDescriptorTable(0, dxComp.descriptorHeap->GetGPUDescriptorHandleForHeapStart());

		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 2);
		d3d.CmdList->Dispatch(
			static_cast<UINT>(ceil(d3d.Width / 32.0f)), 
			static_cast<UINT>(ceil(d3d.Height / 32.0f)),
			1u);

		d3d.CmdList->ResourceBarrier(_countof(dlssUAVBarriers), dlssUAVBarriers);

		if (TAAEnabled)
		{
			D3D12_RESOURCE_BARRIER TAABarriers[2] = {};
			TAABarriers[0].Transition.pResource = resources.DXROutput[1];
			TAABarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			TAABarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

			TAABarriers[1].Transition.pResource = resources.Log2CartOutput;
			TAABarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			TAABarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

			d3d.CmdList->ResourceBarrier(2, TAABarriers);
			d3d.CmdList->CopyResource(resources.DXROutput[1], resources.Log2CartOutput);

			TAABarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			TAABarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			TAABarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			TAABarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			d3d.CmdList->ResourceBarrier(2, TAABarriers);
		}

		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 3);

		//if (clearTAA)
		//{
		//	auto gpuHandle = resources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		//	auto cpuHandle = resources.cpuOnlyHeap->GetCPUDescriptorHandleForHeapStart();

		//	gpuHandle.ptr += 4 * d3d.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		//	//Dont clear if using stochastic sampling method
		//	const float ClearColor[4] = { 0, 0, 0, 0 };
		//	d3d.CmdList->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, resources.DXROutput[1], ClearColor, 0, NULL);
		//}

		//Start DLSS time
		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 4);

		//DLSS Layer
		if (dlssConfig.ShouldUseDLSS)
		{
			D3D12_RESOURCE_BARRIER DLSSInputBarriers[3] = {};

			DLSSInputBarriers[0].Transition.pResource = resources.Log2CartOutput;
			DLSSInputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			DLSSInputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			DLSSInputBarriers[1].Transition.pResource = resources.DLSSDepthInput;
			DLSSInputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			DLSSInputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			DLSSInputBarriers[2].Transition.pResource = resources.FinalMotionOutput;
			DLSSInputBarriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			DLSSInputBarriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			d3d.CmdList->ResourceBarrier(3, DLSSInputBarriers);

			NVSDK_NGX_Coordinates Offset;
			Offset.X = 0;
			Offset.Y = 0;

			NVSDK_NGX_Dimensions SubRect;
			SubRect.Width = d3d.Width;
			SubRect.Height = d3d.Height;

			NVSDK_NGX_D3D12_DLSS_Eval_Params DLSSParams;

			memset(&DLSSParams, 0, sizeof(DLSSParams));

			DLSSParams.Feature.pInColor = resources.Log2CartOutput;
			DLSSParams.Feature.pInOutput = resources.DLSSOutput;
			DLSSParams.pInMotionVectors = resources.FinalMotionOutput;
			DLSSParams.pInDepth = resources.DLSSDepthInput;
			DLSSParams.InJitterOffsetX = -dlssConfig.JitterOffset.X;
			DLSSParams.InJitterOffsetY = -dlssConfig.JitterOffset.Y;
			DLSSParams.Feature.InSharpness = dlssConfig.sharpness;
			DLSSParams.InReset = false;
			DLSSParams.InMVScaleX = 1;
			DLSSParams.InMVScaleY = 1;
			DLSSParams.InColorSubrectBase = Offset;
			DLSSParams.InDepthSubrectBase = Offset;
			DLSSParams.InTranslucencySubrectBase = Offset;
			DLSSParams.InMVSubrectBase = Offset;
			DLSSParams.InRenderSubrectDimensions = SubRect;

			NVSDK_NGX_Result nvr = NGX_D3D12_EVALUATE_DLSS_EXT(d3d.CmdList, dlssConfig.DLSSFeature, dlssConfig.Params, &DLSSParams);
			Utils::ValidateNGX(nvr, "DLSS Evaluation");

			DLSSInputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			DLSSInputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			DLSSInputBarriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			DLSSInputBarriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;


			d3d.CmdList->ResourceBarrier(3, DLSSInputBarriers);
		}
		else
		{
			D3D12_RESOURCE_BARRIER Barrier = {};

			Barrier.Transition.pResource = resources.Log2CartOutput;
			Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

			d3d.CmdList->ResourceBarrier(1, &Barrier);
			d3d.CmdList->ResourceBarrier(1, &OutputBarriers[1]);
			
			d3d.CmdList->CopyResource(resources.DLSSOutput, resources.Log2CartOutput);

			OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			d3d.CmdList->ResourceBarrier(1, &OutputBarriers[1]);
			d3d.CmdList->ResourceBarrier(1, &Barrier);
		}

		//End DLSS time
		d3d.CmdList->EndQuery(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 5);

		//Black out DLSS watermark if taking screenshot
		if (scrshotRequested && dlssPreScrshot)
		{
			d3d.CmdList->SetDescriptorHeaps(1, &dxComp.wmHeap);
			d3d.CmdList->SetPipelineState(dxComp.wmPs);
			d3d.CmdList->SetComputeRootSignature(dxComp.watermarkProgram.pRootSignature);

			d3d.CmdList->SetComputeRootDescriptorTable(0, dxComp.wmHeap->GetGPUDescriptorHandleForHeapStart());

			d3d.CmdList->Dispatch(
				static_cast<UINT>(ceil(d3d.DisplayWidth / 32.0f)),
				static_cast<UINT>(ceil(d3d.DisplayHeight / 32.0f)),
				1u);
		}

		//DLSS output barrier
		d3d.CmdList->ResourceBarrier(1, &uavBarriers[3]);

		// Transition Final output to a copy source
		OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		// Wait for the transitions to complete
		d3d.CmdList->ResourceBarrier(1, &OutputBarriers[1]);

		// Copy the Final output to the back buffer
		d3d.CmdList->CopyResource(d3d.BackBuffer[d3d.FrameIndex], resources.DLSSOutput);
		
		if (scrshotRequested)
		{
			// Describe the upload heap resource location for the copy
			D3D12_SUBRESOURCE_FOOTPRINT subresource = {};
			subresource.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			subresource.Width = d3d.DisplayWidth;
			subresource.Height = d3d.DisplayHeight;
			subresource.RowPitch = (d3d.DisplayWidth * 4);
			subresource.Depth = 1;

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
			footprint.Offset = 0;
			footprint.Footprint = subresource;

			D3D12_TEXTURE_COPY_LOCATION dest = {};
			dest.pResource = resources.OutputReadBack;
			dest.PlacedFootprint = footprint;
			dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

			// Describe the default heap resource location for the copy
			D3D12_TEXTURE_COPY_LOCATION source = {};
			source.pResource = resources.DLSSOutput;
			source.SubresourceIndex = 0;
			source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

			d3d.CmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
		}

		// Transition back buffer to present
		OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		// Wait for the transitions to complete
		d3d.CmdList->ResourceBarrier(1, &OutputBarriers[0]);

		////UI
		auto rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += d3d.FrameIndex * resources.rtvDescSize;

		d3d.CmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);
		d3d.CmdList->SetDescriptorHeaps(1, &resources.uiHeap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d.CmdList);
		OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		d3d.CmdList->ResourceBarrier(1, &OutputBarriers[0]);

		//Resolve all timestamp queries
		d3d.CmdList->ResolveQueryData(resources.queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, 6, resources.TimestampReadBack, 0);

		// Submit the command list and wait for the GPU to idle
		D3D12::Submit_CmdList(d3d);
		D3D12::WaitForGPU(d3d);
	}

	/**
	 * Release DXR resources.
	 */
	void Destroy(DXRGlobal& dxr)
	{
		SAFE_RELEASE(dxr.TLAS.pScratch);
		SAFE_RELEASE(dxr.TLAS.pResult);
		SAFE_RELEASE(dxr.TLAS.pInstanceDesc);
		SAFE_RELEASE(dxr.shaderTable);
		SAFE_RELEASE(dxr.rgs.blob);
		SAFE_RELEASE(dxr.rgs.pRootSignature);
		SAFE_RELEASE(dxr.miss.blob);
		SAFE_RELEASE(dxr.hit.chs.blob);
		SAFE_RELEASE(dxr.rtpso);
		SAFE_RELEASE(dxr.rtpsoInfo);
		SAFE_RELEASE(dxr.BLAS.pScratch);
		SAFE_RELEASE(dxr.BLAS.pResult);
		SAFE_RELEASE(dxr.BLAS.pInstanceDesc);
	}

}