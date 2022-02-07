#include "StaticMesh.h"


void StaticMesh::ReserveNumVertices(uint32_t Num)
{
	Vertices.reserve(Num);
	Normals.reserve(Num);
	Texcoords.reserve(Num);
}


void StaticMesh::AppendVertex(Vector3f& Vertex)
{
	Vertices.push_back(Vertex);
	NumVertices++;
}

void StaticMesh::AppendNormal(Vector3f& Normal)
{
	Normals.push_back(Normal);
}

void StaticMesh::AppendTextureCoord(Vector2f& Texcoord)
{
	Texcoords.push_back(Texcoord);
}

void StaticMesh::SetMaterial(Material& material)
{
	MeshMaterial = material;
	IsMaterialSet = true;
}

bool StaticMesh::HasMaterial()
{
	return IsMaterialSet;
}

bool StaticMesh::HasNormals()
{
	return !Normals.empty();
}

bool StaticMesh::HasTextureCoords()
{
	return !Texcoords.empty();
}

void StaticMesh::Clear()
{
	Vertices.clear();
	Normals.clear();
	Texcoords.clear();
	MeshMaterial = {};
	IsMaterialSet = false;
}

StaticMesh::~StaticMesh()
{
	Clear();
}