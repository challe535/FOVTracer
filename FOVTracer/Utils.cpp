#include "Utils.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
		uint32_t LoadFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded |
			aiProcess_FlipUVs | aiProcess_FlipWindingOrder;

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

							Vertex Vert;
							Vec = pMesh->mVertices[j];
							Vector3f VertPos(Vec.x, Vec.y, Vec.z);
							Vert.Position = VertPos;
							Vert.Texcoord = Texcoord;

							SMesh.Verts.push_back(Vert);
						}
					}

					if (pMesh->HasFaces())
					{
						for (j = 0; j < pMesh->mNumFaces; j++)
						{
							for (uint32_t k = 0; k < pMesh->mFaces[j].mNumIndices; k++)
							{
								uint32_t Index = pMesh->mFaces[j].mIndices[k];
								SMesh.Indices.push_back(Index);
							}
						}
					}
					else
					{
						CORE_ERROR("Missing indices for mesh at {0}!", Filepath);
						return false;
					}

					//Set material. Currently single material with texture only.
					if (pScene->HasMaterials())
					{
						aiMaterial* pMat = pScene->mMaterials[pMesh->mMaterialIndex];
						Material Mat;

						Mat.Name = pMat->GetName().C_Str();


						//aiString TexturePath;
						//pMat->GetTexture(aiTextureType_DIFFUSE, 0, &TexturePath);
						//	
						CORE_WARN("Material name is {0}", Mat.Name);

						Mat.TexturePath = std::string(PATH_TO_RESOURCES).append("textures/statue.jpg");

						SMesh.SetMaterial(Mat);
					}
					SMeshVector.push_back(SMesh);
				}
				else
				{
					CORE_ERROR("Mesh at {0} has no positions!", Filepath);
					return false;
				}
			}
		}
		else
		{
			CORE_ERROR("Failed to load scene at {0}!", Filepath);
			return false;
		}

		return true;
	}

	std::string GetResourcePath(const std::string& ResourceName)
	{
		return std::string(PATH_TO_RESOURCES).append(ResourceName);
	}

	/**
	* Load an image
	*/
	TextureInfo LoadTexture(std::string filepath)
	{
		TextureInfo result = {};

		// Load image pixels with stb_image
		UINT8* pixels = stbi_load(filepath.c_str(), &result.width, &result.height, &result.stride, STBI_default);
		if (!pixels)
		{
			throw std::runtime_error("Error: failed to load image!");
		}

		FormatTexture(result, pixels);
		stbi_image_free(pixels);
		return result;
	}

	/**
	* Format the loaded texture into the layout we use with D3D12.
	*/
	void FormatTexture(TextureInfo& info, UINT8* pixels)
	{
		const UINT numPixels = (info.width * info.height);
		const UINT oldStride = info.stride;
		const UINT oldSize = (numPixels * info.stride);

		const UINT newStride = 4;				// uploading textures to GPU as DXGI_FORMAT_R8G8B8A8_UNORM
		const UINT newSize = (numPixels * newStride);
		info.pixels.resize(newSize);

		for (UINT i = 0; i < numPixels; i++)
		{
			info.pixels[i * newStride] = pixels[i * oldStride];		// R
			info.pixels[i * newStride + 1] = pixels[i * oldStride + 1];	// G
			info.pixels[i * newStride + 2] = pixels[i * oldStride + 2];	// B
			info.pixels[i * newStride + 3] = 0xFF;							// A (always 1)
		}

		info.stride = newStride;
	}

}