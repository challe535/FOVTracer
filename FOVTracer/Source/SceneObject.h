#pragma once

#include "Transform.h"
#include "StaticMesh.h"
#include <vector>

class SceneObject
{
public:

	Transform Transform;
	StaticMesh Mesh;

	~SceneObject();


	bool LoadFromPath(const std::string& Path, bool GenNormals);

	void Clear();
	//StaticMesh SingleMeshRepresentation;
private:
};

