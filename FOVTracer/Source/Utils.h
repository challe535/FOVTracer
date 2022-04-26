#pragma once

#include "Platform.h"
#include "StaticMesh.h"
#include "dlss/nvsdk_ngx.h"

#include "DirectXTex/DirectXTex.h"

#include "DX.h"

#define PATH_TO_RESOURCES "./Resources/"

namespace Utils
{
	/**
	* Get a resource from the resource path specified by PATH_TO_RESOURCES.
	*/
	std::string GetResourcePath(const std::string& ResourceName);

	/**
	* Checks whether HRESULT is an error and creates a error message dialouge if so.
	*/
	void Validate(HRESULT hr, LPCWSTR msg);

	void ValidateNGX(NVSDK_NGX_Result nvr, std::string msg);

	/**
	* Loads meshes from filepath into SMeshVector.
	*/
	bool LoadStaticMeshes(const std::string& Filepath, std::vector<StaticMesh>& SMeshVector, bool GenVertexNormals);

	/**
	* Loads a texture with stb_image and returns its data.
	*/
	TextureInfo LoadTexture(std::string filepath, D3D12Resources& resources, UINT channelBytes);

	void FormatTexture(TextureInfo& info, UINT8* pixels, UINT newStride);

	TextureInfo GetDecompressedAndConvertedImage(DirectX::ScratchImage& SrcImage);

	void FillTextureInfoFromScratchImage(DirectX::ScratchImage& Image, TextureInfo& TInfo);

	struct SFallbackTexture
	{
		UINT8* texture;
		UINT32 width = 256;
		UINT32 height = 256;
		UINT32 stride = 4;
	};
	static SFallbackTexture* FallbackTexture = new SFallbackTexture();

	SFallbackTexture* GetFallbackTexture();

	bool DumpPNG(const char* name, int width, int height, int comp, const void* data);
}
