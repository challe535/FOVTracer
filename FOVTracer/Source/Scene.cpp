#include "pch.h"
#include "Scene.h"
#include "Utils.h"

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

void Scene::LoadFromPath(std::string Path)
{
	std::vector<StaticMesh> Meshes;

	bool Success = Utils::LoadStaticMeshes(Path, Meshes, true);

	if (Success)
	{
		for (int i = 0; i < Meshes.size(); i++)
		{
			SceneObject ScnObj;
			ScnObj.Mesh = Meshes[i];

			SceneObjects.push_back(ScnObj);
		}
	}
}
