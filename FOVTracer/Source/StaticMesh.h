#pragma once

#include "Math.h"

#include <string>
#include <vector>

struct Material
{
	std::string Name = "DefaultMaterial";
	std::string TexturePath = "";
	std::string NormalMapPath = "";
	std::string OpacityMapPath = "";
	Vector2f  TextureResolution = Vector2f(512, 512);

	Vector3f AmbientColor;
	Vector3f DiffuseColor;
	Vector3f SpecularColor;
	Vector3f TransmitanceFilter;
	float Shininess = 1.0f;
	float RefractIndex = 0.0f;
	float TF = 0.0f;
};

struct Vertex
{
	Vector3f Position;
	Vector2f Texcoord;
	Vector3f Normal;
	Vector3f Tangent;
	Vector3f Binormal;
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
	bool HasTangents = false;
	bool HasBinormals = false;
	bool HasTransparency = false;

private:

};

