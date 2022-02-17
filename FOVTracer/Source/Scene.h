#pragma once

#include "SceneObject.h"

#include <vector>
#include <string>

class Scene
{
public:
	void AddSceneObject(const SceneObject& SObject);
	uint32_t GetNumSceneObjects();

	void LoadFromPath(std::string Path, bool ShouldGenNormals);

	void Clear();

	std::vector<SceneObject> SceneObjects;
private:
	//std::vector<PointLight> ScenePointLights;
};

