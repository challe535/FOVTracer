#pragma once

#include "SceneObject.h"

#include <vector>
#include <string>

class Scene
{
public:
	void AddSceneObject(const SceneObject& SObject);
	uint32_t GetNumSceneObjects();

	void Clear();
private:
	std::vector<SceneObject> SceneObjects;
	//std::vector<PointLight> ScenePointLights;
};

