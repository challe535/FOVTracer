#pragma once

#include "Platform.h"
#include "StaticMesh.h"

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

	/**
	* Loads meshes from filepath into SMeshVector.
	*/
	bool LoadStaticMeshes(const std::string& Filepath, std::vector<StaticMesh>& SMeshVector, bool GenVertexNormals);

	/**
	* Loads a texture with stb_image and returns its data.
	*/
	TextureInfo LoadTexture(std::string filepath, D3D12Resources& resources);

	void FormatTexture(TextureInfo& info, UINT8* pixels);

	struct SFallbackTexture
	{
		UINT8* texture;
		UINT32 width = 256;
		UINT32 height = 256;
		UINT32 stride = 4;
	};
	static SFallbackTexture* FallbackTexture = new SFallbackTexture();

	SFallbackTexture* GetFallbackTexture();
}
