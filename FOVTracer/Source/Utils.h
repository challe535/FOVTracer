#pragma once

#include "Platform.h"
#include "StaticMesh.h"

//REFACTOR SO THIS IS NOT NEEDED. ALL OF DX.h NEEDS TO BE REVISITED AND REFACTORED!!!
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
	TextureInfo LoadTexture(std::string filepath);

	void FormatTexture(TextureInfo& info, UINT8* pixels);
}
