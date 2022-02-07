#include "Utils.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"

#include "Log.h"

namespace Utils
{
	void Validate(HRESULT hr, LPCWSTR msg)
	{
		if (FAILED(hr))
		{
			MessageBox(NULL, msg, L"Error", MB_OK);
			PostQuitMessage(EXIT_FAILURE);
		}
	}


	bool LoadStaticMeshes(const std::string& Filepath, std::vector<StaticMesh>& SMeshVector)
	{
		uint32_t LoadFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ConvertToLeftHanded;

		Assimp::Importer Importer;
		const aiScene* pScene = Importer.ReadFile(Filepath.c_str(), LoadFlags);

		if (pScene && pScene->HasMeshes())
		{
			//reserve memory for new meshes
			SMeshVector.reserve(SMeshVector.size() + pScene->mNumMeshes);

			//Create static meshes
			for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
			{
				unsigned int j = 0;

				aiMesh* pMesh = pScene->mMeshes[i];
				StaticMesh SMesh;

				//Set vertex positions and attributes if present
				if (pMesh->HasPositions())
				{
					SMesh.ReserveNumVertices(pMesh->mNumVertices);

					for (j = 0; j < pMesh->mNumVertices; j++)
					{

						aiVector3D Vec = pMesh->mVertices[j];
						Vector3f VertPos(Vec.x, Vec.y, Vec.z);
						SMesh.AppendVertex(VertPos);
					}

					//Set vertex normals
					if (pMesh->HasNormals())
					{
						for (j = 0; j < pMesh->mNumVertices; j++)
						{
							aiVector3D Vec = pMesh->mNormals[j];
							Vector3f Normal(Vec.x, Vec.y, Vec.z);
							SMesh.AppendNormal(Normal);
						}
					}

					//Set vertex texcoords. Single channel supported for now.
					if (pMesh->HasTextureCoords(0))
					{
						for (j = 0; j < pMesh->mNumVertices; j++)
						{
							aiVector3D Vec = pMesh->mTextureCoords[0][j];
							Vector2f Texcoord(Vec.x, Vec.y);
							SMesh.AppendTextureCoord(Texcoord);
						}
					}

					//Set material. Currently single material with texture only.
					if (pScene->HasMaterials())
					{
						aiMaterial* pMat = pScene->mMaterials[pMesh->mMaterialIndex];
						Material Mat;

						Mat.Name = pMat->GetName().C_Str();
						
						if (pMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
						{
							aiTexture Texture;
							pMat->Get<aiTexture>(AI_MATKEY_COLOR_DIFFUSE, Texture);

							Mat.TexturePath = Texture.mFilename.C_Str();
							Mat.TextureResolution = static_cast<float>(Texture.mWidth);
						}

						SMesh.SetMaterial(Mat);
					}

					SMeshVector.push_back(SMesh);
				}
				else
				{
					LUM_CORE_ERROR("Mesh at {0} has no positions!", Filepath);
					return false;
				}
			}
		}
		else
		{
			LUM_CORE_ERROR("Failed to load scene at {0}!", Filepath);
			return false;
		}

		return true;
	}

	std::string GetResourcePath(const std::string& ResourceName)
	{
		return std::string(PATH_TO_RESOURCES).append(ResourceName);
	}

}