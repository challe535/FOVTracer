#pragma once

#include "Math.h"

#include <string>
#include <vector>

struct Material
{
	std::string Name = "DefaultMaterial";
	std::string TexturePath = "";
	float  TextureResolution = 512;
};

struct Vertex
{
	Vector3f Position;
	Vector2f Texcoord;
};

class StaticMesh
{
public:
	~StaticMesh();

	void ReserveNumVertices(uint32_t Num);

	void AppendVertex(Vector3f& Vertex);
	void AppendNormal(Vector3f& Normal);
	void AppendTextureCoord(Vector2f& Texcoord);

	void SetMaterial(Material& material);

	bool HasMaterial();
	bool HasNormals();
	bool HasTextureCoords();

	uint32_t NumVertices;

	void Clear();

	std::vector<Vertex> Verts;

	std::vector<Vector3f> Vertices;
	std::vector<Vector3f> Normals;
	std::vector<Vector2f> Texcoords;
	std::vector<uint32_t> Indices;

	Material MeshMaterial;
	bool IsMaterialSet = false;

private:

};

