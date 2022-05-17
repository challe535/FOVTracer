
#define PI 3.141592653589793
#define GOLDEN 1.61803398875
#define K 1

struct Node
{
    float3 color;
    float depth;
    float transmit;
};

struct HitInfo
{
    float4 ShadedColorAndHitT;
    Node node[K];
    uint RecursionDepthRemaining;
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
	float2 displayResolution;

    float2 jitterOffset;
};

struct MaterialCB
{
	float4 textureResolution;
    
    float3 AmbientColor;
    bool hasDiffuseTexture;
    
    float3 DiffuseColor;
    bool hasNormalMap;
    
    float3 SpecularColor;
    bool hasTransparency;
    
    float3 TransmitanceFilter;
    float Shininess;
    
    float RefractIndex;
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
    bool isDLSSEnabled;

    uint recursionDepth;
    bool useIndirectIllum;
    float2 lastFovealCenter;
    
    float rayTMax;
    bool flipNormals;
    bool takingReferenceScreenshot;
    float foveationAreaThreshold;
    
    uint frameCount;
};

ConstantBuffer<TraceParamsCB> params : register(b2);

RWTexture2D<float4> RTOutput				: register(u0);
RWTexture2D<float4> RTOutput1				: register(u1);
RWTexture2D<float4> RTOutput2				: register(u2);
RWTexture2D<float4> RTOutput3				: register(u3);
RWTexture2D<float4> RTOutput4				: register(u4);
RWTexture2D<float4> MotionOutput            : register(u5);
RWTexture2D<float4> MotionOutput1           : register(u6);
RWTexture2D<float4> WorldPosBuffer          : register(u7);
RWTexture2D<float4> WorldPosBuffer1         : register(u8);

RaytracingAccelerationStructure SceneBVH	: register(t0);

ByteAddressBuffer indices					: register(t1);
ByteAddressBuffer vertices					: register(t2);
Texture2D<float4> albedo					: register(t3);
Texture2D<float4> normals					: register(t4);
Texture2D<float4> opacity					: register(t5);
Texture2D<float4> blueNoise					: register(t6);

SamplerState BilinearClamp : register(s0);

struct VertexAttributes
{
	float3 position;
	float2 uv;
	float3 normal;
    float3 tangent;
    float3 binormal;
    float3 triCross;
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

    float3 vps[3] = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };

	for (uint i = 0; i < 3; i++)
	{
		int address = (indices[i] * 14) * 4;
        float3 vpos = asfloat(vertices.Load3(address));
        v.position += vpos * barycentrics[i];
        vps[i] = vpos;
		address += (3 * 4);
		v.uv += asfloat(vertices.Load2(address)) * barycentrics[i];
		address += (2 * 4);
		v.normal += asfloat(vertices.Load3(address)) * barycentrics[i];
        address += (3 * 4);
        v.tangent += asfloat(vertices.Load3(address)) * barycentrics[i];
        address += (3 * 4);
        v.binormal += asfloat(vertices.Load3(address)) * barycentrics[i];
    }

    v.triCross = cross(vps[1] - vps[0], vps[2] - vps[0]);

	return v;
}

float4 getClip(float3 worldPos, float aspect)
{
    float3 worldDelta = worldPos - viewOriginAndTanHalfFovY.xyz;
    float4 clip = mul(view, float4(worldDelta, 0));

    clip.x /= aspect;
    clip.xy /= viewOriginAndTanHalfFovY.w * clip.z;
    clip.y = -clip.y;

    clip.xy = clip.xy * 0.5 + 0.5;
    
    return clip;
}