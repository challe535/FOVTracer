#include "SceneObject.h"
#include "Utils.h"
#include "Log.h"

bool SceneObject::LoadFromPath(const std::string& Path)
{
	bool Success = Utils::LoadStaticMeshes(Path, StaticMeshes);

	if (Success)
	{
		LUM_CORE_INFO("Successfully loaded {0} static meshes from {1}.", StaticMeshes.size(), Path);

		uint32_t MeshID = 0;
		for (auto it = StaticMeshes.begin(); it < StaticMeshes.end(); it++)
		{
			MeshID++;
			StaticMesh& Mesh = *it;

			LUM_CORE_INFO("Mesh {0}:", MeshID);
			LUM_CORE_INFO("Number of vertices: {0}", Mesh.NumVertices);
			LUM_CORE_INFO("Has normals: {0}", Mesh.HasNormals());
			LUM_CORE_INFO("Has texcoords: {0}", Mesh.HasTextureCoords());
			LUM_CORE_INFO("Has material: {0}", Mesh.HasMaterial());
		}
	}

	return Success;
}

void SceneObject::Clear()
{
	StaticMeshes.clear();
}

SceneObject::~SceneObject()
{
	Clear();
}