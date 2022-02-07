#include "Scene.h"

void Scene::AddSceneObject(const SceneObject& SObject)
{
	SceneObjects.push_back(SObject);
}

uint32_t Scene::GetNumSceneObjects()
{
	return static_cast<uint32_t>(SceneObjects.size());
}

void Scene::Clear()
{
	SceneObjects.clear();
}
