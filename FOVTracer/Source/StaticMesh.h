#pragma once

#include "Math.h"

#include <string>
#include <vector>

struct Material
{
	std::string Name = "DefaultMaterial";
	std::string TexturePath = "";
	Vector2f  TextureResolution = Vector2f(512, 512);
};

struct Vertex
{
	Vector3f Position;
	Vector2f Texcoord;
	Vector3f Normal;
};

class StaticMesh
{
public:
	void ReserveNumVertices(uint32_t Num);

	void Clear();

	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	Material MeshMaterial;

	bool HasMaterial = false;
	bool HasNormals = false;
	bool HasTexcoords = false;

private:

};

