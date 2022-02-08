#include "pch.h"
#include "StaticMesh.h"


void StaticMesh::ReserveNumVertices(uint32_t Num)
{
	Vertices.reserve(Num);
}

void StaticMesh::Clear()
{
	Vertices.clear();
	MeshMaterial = {};

	HasMaterial = false;
	HasNormals = false;
	HasTexcoords = false;
}