#include "pch.h"
#include "Utils.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

	void ValidateNGX(NVSDK_NGX_Result nvr, std::string msg)
	{
		if (NVSDK_NGX_FAILED(nvr))
		{
			switch (nvr)
			{
			case NVSDK_NGX_Result_FAIL_FeatureNotSupported:
				CORE_ERROR("{0} :: Feature not supported", msg);
				break;

			case NVSDK_NGX_Result_FAIL_PlatformError:
				CORE_ERROR("{0} :: Platform error", msg);
				break;
			
			case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists:
				CORE_ERROR("{0} :: Feature already exists", msg);
				break;

			case NVSDK_NGX_Result_FAIL_FeatureNotFound:
				CORE_ERROR("{0} :: Feature not found", msg);
				break;

			case NVSDK_NGX_Result_FAIL_InvalidParameter:
				CORE_ERROR("{0} :: Invalid parameter", msg);
				break;

			case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall:
				CORE_ERROR("{0} :: Scratch buffer too small", msg);
				break;

			case NVSDK_NGX_Result_FAIL_NotInitialized:
				CORE_ERROR("{0} :: SDK not initialized properly", msg);
				break;

			case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
				CORE_ERROR("{0} :: Unsupported format for inout buffers", msg);
				break;

			case NVSDK_NGX_Result_FAIL_RWFlagMissing:
				CORE_ERROR("{0} :: Feature input/output needs RW access (UAV)", msg);
				break;

			case NVSDK_NGX_Result_FAIL_MissingInput:
				CORE_ERROR("{0} :: Feature was created with specific input but none is provided at evaluation", msg);
				break;

			case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature:
				CORE_ERROR("{0} :: Feature is not available on the system", msg);
				break;

			case NVSDK_NGX_Result_FAIL_OutOfDate:
				CORE_ERROR("{0} :: NGX system libraries are old and need an update", msg);
				break;

			case NVSDK_NGX_Result_FAIL_OutOfGPUMemory:
				CORE_ERROR("{0} :: Feature requires more GPU memory than it is available on system", msg);
				break;

			case NVSDK_NGX_Result_FAIL_UnsupportedFormat:
				CORE_ERROR("{0} :: Format used in input buffer(s) is not supported by feature", msg);
				break;

			case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath:
				CORE_ERROR("{0} :: Path provided in InApplicationDataPath cannot be written to", msg);
				break;

			case NVSDK_NGX_Result_FAIL_UnsupportedParameter:
				CORE_ERROR("{0} :: Unsupported parameter was provided (e.g. specific scaling factor is unsupported)", msg);
				break;

			case NVSDK_NGX_Result_FAIL_Denied:
				CORE_ERROR("{0} :: The feature or application was denied", msg);
				break;
			}
		}
	}

	//TODO: Make recursively process nodes if needed by important scenes later.
	bool LoadStaticMeshes(const std::string& Filepath, std::vector<StaticMesh>& SMeshVector, bool GenVertexNormals)
	{
		bool LoadSucceeded = true;

		uint32_t LoadFlags = aiProcess_Triangulate | aiProcess_MakeLeftHanded |
			aiProcess_FlipUVs | aiProcess_FlipWindingOrder | aiProcess_SortByPType | aiProcess_CalcTangentSpace;

		if (GenVertexNormals)
			LoadFlags |= aiProcess_GenSmoothNormals;

		Assimp::Importer Importer;
		const aiScene* pScene = Importer.ReadFile(Filepath.c_str(), LoadFlags);

		if (pScene && pScene->HasMeshes())
		{
			//reserve memory for new meshes
			SMeshVector.reserve(SMeshVector.size() + pScene->mNumMeshes);

			//Create static meshes
			for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
			{
				aiMesh* pMesh = pScene->mMeshes[i];
			
				if (!pMesh->HasPositions())
				{
					CORE_ERROR("Mesh {0} from file \"{1}\" is missing vertex positions!", i, Filepath);
					LoadSucceeded = false;
					continue;
				}

				unsigned int j = 0;
				StaticMesh SMesh;
				SMesh.ReserveNumVertices(pMesh->mNumVertices);

				//Create vertices
				for (j = 0; j < pMesh->mNumVertices; j++)
				{
					Vertex Vtx;

					aiVector3D Vec = pMesh->mVertices[j];
					Vector3f VertPos(Vec.x, Vec.y, Vec.z);
					Vtx.Position = VertPos;
						
					//Texture coords. Only single channel.
					if (pMesh->HasTextureCoords(0))
					{
						SMesh.HasTexcoords = true;

						aiVector3D Vec = pMesh->mTextureCoords[0][j];
						Vector2f Texcoord(Vec.x, Vec.y);
						
						Vtx.Texcoord = Texcoord;
					}

					//Vertex normals.
					if (pMesh->HasNormals())
					{
						SMesh.HasNormals = true;

						aiVector3D Vec = pMesh->mNormals[j];
						Vector3f Normal(Vec.x, Vec.y, Vec.z);
						Vtx.Normal = Normal;
					}

					//Vertex tangents and binormals.
					if (pMesh->HasTangentsAndBitangents())
					{
						SMesh.HasTangents = true;
						SMesh.HasBinormals = true;

						aiVector3D Vec = pMesh->mTangents[j];
						Vector3f Tangent(Vec.x, Vec.y, Vec.z);
						Vtx.Tangent = Tangent;

						Vec = pMesh->mBitangents[j];
						Vector3f Bitangent(Vec.x, Vec.y, Vec.z);
						Vtx.Binormal = Bitangent;
					}

					SMesh.Vertices.push_back(Vtx);
				}

				//Set indices
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
					CORE_ERROR("Missing indices for mesh {0} at {1}!", i, Filepath);
					LoadSucceeded = false;
					continue;
				}
					
				//Set material. Currently single material with texture only.
				if (pScene->HasMaterials())
				{

					auto const end_of_basedir = Filepath.rfind("/");
					auto const parent_folder = (end_of_basedir != std::string::npos ? Filepath.substr(0, end_of_basedir) : ".") + "/";

					SMesh.HasMaterial = true;

					aiMaterial* pMat = pScene->mMaterials[pMesh->mMaterialIndex];
					Material Mat;

					Mat.Name = pMat->GetName().C_Str();

					if (pMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
					{
						aiString path;
						pMat->GetTexture(aiTextureType_DIFFUSE, 0, &path);

						Mat.TexturePath = parent_folder + std::string(path.C_Str());
					}

					if (pMat->GetTextureCount(aiTextureType_NORMALS) > 0)
					{
						aiString path;
						pMat->GetTexture(aiTextureType_NORMALS, 0, &path);

						Mat.NormalMapPath = parent_folder + std::string(path.C_Str());
					}


					if (pMat->GetTextureCount(aiTextureType_OPACITY) > 0)
					{
						SMesh.HasTransparency = true;

						aiString path;
						pMat->GetTexture(aiTextureType_OPACITY, 0, &path);

						Mat.OpacityMapPath = parent_folder + std::string(path.C_Str());
					}

					aiColor3D ColorResult;
					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_COLOR_AMBIENT, ColorResult))
						Mat.AmbientColor = Vector3f(ColorResult.r, ColorResult.g, ColorResult.b);

					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_COLOR_DIFFUSE, ColorResult))
						Mat.DiffuseColor = Vector3f(ColorResult.r, ColorResult.g, ColorResult.b);

					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_COLOR_SPECULAR, ColorResult))
						Mat.SpecularColor = Vector3f(ColorResult.r, ColorResult.g, ColorResult.b);

					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_COLOR_TRANSPARENT, ColorResult))
						Mat.TransmitanceFilter = Vector3f(ColorResult.r, ColorResult.g, ColorResult.b);

					ai_real FloatResult;
					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_SHININESS, FloatResult))
						Mat.Shininess = FloatResult;

					if (aiReturn_SUCCESS == pMat->Get(AI_MATKEY_REFRACTI, FloatResult))
						Mat.RefractIndex = FloatResult;

					SMesh.MeshMaterial = Mat;
				}
				else
				{
					CORE_ERROR("Missing material for mesh {0} at {1}!", i, Filepath);
					LoadSucceeded = false;
					continue;
				}

				CORE_TRACE("Loaded mesh {0} with {1} tris :: Normals {2}, Texcoords {3}, Material {4}",
					pMesh->mName.C_Str(),
					SMesh.Indices.size() / 3,
					SMesh.HasNormals ? "available" : "N/A",
					SMesh.HasTexcoords ? "available" : "N/A",
					SMesh.HasMaterial ? "available" : "N/A"
					);
				SMeshVector.push_back(SMesh);
			}
		}
		else
		{
			CORE_ERROR("Failed to load scene at {0}!", Filepath);
			LoadSucceeded = false;
		}

		return LoadSucceeded;
	}

	std::string GetResourcePath(const std::string& ResourceName)
	{
		return std::string(PATH_TO_RESOURCES).append(ResourceName);
	}

	SFallbackTexture* GetFallbackTexture()
	{
		if (FallbackTexture->texture)
			return FallbackTexture;

		FallbackTexture->texture = reinterpret_cast<UINT8*>(malloc(FallbackTexture->width * FallbackTexture->height * FallbackTexture->stride));
		const UINT8 fallbackRed = 0xFF;
		const UINT8 fallbackGreen = 0;
		const UINT8 fallbackBlue = 0xFF;
		const UINT8 fallbackAlpha = 0xFF;

		for (uint32_t i = 0; i < FallbackTexture->width * FallbackTexture->height; i++)
		{
			FallbackTexture->texture[i * FallbackTexture->stride] = fallbackRed;
			FallbackTexture->texture[i * FallbackTexture->stride + 1] = fallbackGreen;
			FallbackTexture->texture[i * FallbackTexture->stride + 2] = fallbackBlue;
			FallbackTexture->texture[i * FallbackTexture->stride + 3] = fallbackAlpha;
		}

		return FallbackTexture;
	}

	/**
	* Load an image
	*/
	TextureInfo LoadTexture(std::string filepath, D3D12Resources& resources, UINT channelBytes)
	{
		//if (resources.Textures.count(filepath) > 0)
		//	return resources.Textures.at(filepath).textureInfo;

		TextureInfo result = {};

		CORE_TRACE("Attempting to loading texture from {0}", filepath);

		// Load image pixels with stb_image
		UINT8* pixels = stbi_load(filepath.c_str(), &result.width, &result.height, &result.stride, STBI_default);
		if (!pixels)
		{
			CORE_ERROR("Failed to load texture at {0}", filepath);
			SFallbackTexture* fallback = GetFallbackTexture();

			pixels = fallback->texture;
			result.width = fallback->width;
			result.height = fallback->height;
			result.stride = fallback->stride;
		}

		FormatTexture(result, pixels, channelBytes);
		stbi_image_free(pixels);

		return result;
	}

	/**
	* Format the loaded texture into the layout we use with D3D12.
	*/
	void FormatTexture(TextureInfo& info, UINT8* pixels, UINT newStride)
	{
		const UINT numPixels = (info.width * info.height);
		const UINT oldStride = info.stride;
		const UINT oldSize = (numPixels * info.stride);

		//const UINT newStride = 4;				// uploading textures to GPU as DXGI_FORMAT_R8G8B8A8_UNORM
		const UINT newSize = (numPixels * newStride);
		info.pixels.resize(newSize);

		for (UINT i = 0; i < numPixels; i++)
		{
			info.pixels[i * newStride] = pixels[i * oldStride];		// R
			info.pixels[i * newStride + 1] = pixels[i * oldStride + 1];	// G
			info.pixels[i * newStride + 2] = pixels[i * oldStride + 2];	// B
			info.pixels[i * newStride + 3] = pixels[i * oldStride + 3];	// A
		}

		info.stride = newStride;
	}

	bool DumpPNG(const char* name, int width, int height, int comp, const void* data)
	{
		return stbi_write_png(name, width, height, comp, data, 0);
	}
}