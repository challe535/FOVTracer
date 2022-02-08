#include "pch.h"
#include "SceneObject.h"
#include "Utils.h"
#include "Log.h"

bool SceneObject::LoadFromPath(const std::string& Path)
{
	bool Success = Utils::LoadStaticMeshes(Path, StaticMeshes);

	if (Success)
	{
		CORE_TRACE("Successfully loaded {0} static meshes from {1}.", StaticMeshes.size(), Path);

		uint32_t MeshID = 0;
		for (auto it = StaticMeshes.begin(); it < StaticMeshes.end(); it++)
		{
			MeshID++;
			StaticMesh& Mesh = *it;

			CORE_TRACE("Mesh {0}:", MeshID);
			CORE_TRACE("Number of vertices: {0}", Mesh.Vertices.size());
			CORE_TRACE("Has normals: {0}", Mesh.HasNormals);
			CORE_TRACE("Has texcoords: {0}", Mesh.HasTexcoords);
			CORE_TRACE("Has material: {0}", Mesh.HasMaterial);

			CORE_TRACE("Material texture path: {0}", Mesh.MeshMaterial.TexturePath);
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