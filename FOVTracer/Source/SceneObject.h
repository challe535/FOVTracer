#pragma once

#include "Transform.h"
#include "StaticMesh.h"
#include <vector>

class SceneObject
{
public:
	~SceneObject();

	Transform Transform;

	bool LoadFromPath(const std::string& Path);

	void Clear();

	std::vector<StaticMesh> StaticMeshes;
private:
};

