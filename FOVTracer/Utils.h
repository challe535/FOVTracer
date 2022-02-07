#pragma once

#include "Platform.h"
#include "StaticMesh.h"


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
	bool LoadStaticMeshes(const std::string& Filepath, std::vector<StaticMesh>& SMeshVector);
}
