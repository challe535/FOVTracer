
#define PI 3.141592653589793
#define K 10

struct Node
{
    float depth;
    float transmit;
    float3 color;
};

struct HitInfo
{
    float4 ShadedColorAndHitT;
    Node node[K];
};

struct ShadowHitInfo
{
	bool isHit;
};

struct Attributes 
{
	float2 uv;
};

cbuffer ViewCB : register(b0)
{
	matrix view;
	float4 viewOriginAndTanHalfFovY;
	float2 resolution;

    matrix lastView;
    float4 lastViewOriginAndTanHalfFovY;
};

struct MaterialCB
{
	float4 textureResolution;
    bool hasDiffuseTexture;
    bool hasNormalMap;
    bool hasTransparency;
};

ConstantBuffer<MaterialCB> material : register(b1);

struct TraceParamsCB
{
    float elapsedTimeSeconds;
    uint sqrtSamplesPerPixel;
    float2 fovealCenter;
    bool isFoveatedRenderingEnabled;
    float kernelAlpha;
    float viewportRatio;
    float foveationFillOffset;
};

ConstantBuffer<TraceParamsCB> params : register(b2);

RWTexture2D<float4> RTOutput				: register(u0);
RWTexture2D<float4> MotionOutput            : register(u1);
RWTexture2D<float4> WorldPosBuffer          : register(u2);

RaytracingAccelerationStructure SceneBVH	: register(t0);

ByteAddressBuffer indices					: register(t1);
ByteAddressBuffer vertices					: register(t2);
Texture2D<float4> albedo					: register(t3);
Texture2D<float4> normals					: register(t4);
Texture2D<float4> opacity					: register(t5);

struct VertexAttributes
{
	float3 position;
	float2 uv;
	float3 normal;
    float3 tangent;
    float3 binormal;
};

uint3 GetIndices(uint triangleIndex)
{
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4);
	return indices.Load3(address);
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics)
{
	uint3 indices = GetIndices(triangleIndex);
	VertexAttributes v;
	v.position = float3(0, 0, 0);
	v.uv = float2(0, 0);
	v.normal = float3(0, 0, 0);
    v.tangent = float3(0, 0, 0);
    v.binormal = float3(0, 0, 0);

	for (uint i = 0; i < 3; i++)
	{
		int address = (indices[i] * 14) * 4;
		v.position += asfloat(vertices.Load3(address)) * barycentrics[i];
		address += (3 * 4);
		v.uv += asfloat(vertices.Load2(address)) * barycentrics[i];
		address += (2 * 4);
		v.normal += asfloat(vertices.Load3(address)) * barycentrics[i];
        address += (3 * 4);
        v.tangent += asfloat(vertices.Load3(address)) * barycentrics[i];
        address += (3 * 4);
        v.binormal += asfloat(vertices.Load3(address)) * barycentrics[i];
    }

	return v;
}