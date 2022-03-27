#include "Common.hlsl"
#include "KernelFov.hlsl"

struct PointLightInfo
{
    float3 position;
    float3 color;
    float luminocity;
};

bool IsShadowed(float3 lightDir, float3 origin, float maxDist, float3 normal)
{
    if (dot(normal, lightDir) < 0)
        return true;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = maxDist;

    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = true;

    TraceRay(
		SceneBVH,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		shadowPayload
	);

    return shadowPayload.isHit;
}

float3 CalcMappedNormal(VertexAttributes vertex, int2 coord)
{
    float3 mapping = (normals.Load(int3(coord, 0)).rgb - 0.5) * 2;
    return mapping.x * vertex.tangent + mapping.y * vertex.binormal + mapping.z * vertex.normal;
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

    int2 coord = floor(frac(vertex.uv) * material.textureResolution.xy);

    float2 pixelSize = 1.0f/DispatchRaysDimensions();
    float2 indexNorm = float2(DispatchRaysIndex().xy) * pixelSize;

    float maxSize = max(pixelSize.x, pixelSize.y);
    float3 vBase = normalize(WorldRayDirection()) * length(view[2].xyz);
    float3 v1 = normalize(vBase + normalize(view[0].xyz) * maxSize);
    float a = 2 * (sqrt(-(dot(normalize(WorldRayDirection()), v1) - 1) * 2));

    float coneFactor = a * (RayTCurrent() + 100) / abs(dot(normalize(vertex.triCross), WorldRayDirection()));
    float lodBias = 0;

    if (params.isFoveatedRenderingEnabled)
        lodBias += 3 * pow(smoothstep(0, 1, kernelFunc(indexNorm.x, params.kernelAlpha)), 3);

    if (params.isDLSSEnabled)
        lodBias += log2(DispatchRaysDimensions().x / displayResolution.x) - 1.0f + 0.0001;

    float lod = clamp(lodBias + log2(coneFactor), 0, 9);

    float4 diffuse = float4(1, 1, 0, 1);
    if (material.hasDiffuseTexture)
    {
        //diffuse = albedo.Load(int3(coord, 0));
        diffuse = albedo.SampleLevel(BilinearClamp, vertex.uv, lod);
    }

    if (material.hasNormalMap)
    {
        vertex.normal = CalcMappedNormal(vertex, coord);
    }

    // --- Light ---
    PointLightInfo plInfo;

    plInfo.position = float3(0, 700 /*+ 100 * sin(params.elapsedTimeSeconds)*/, 0);
    plInfo.color = float3(0.9, 0.9, 1.0);
    plInfo.luminocity = 1000000.0;

    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.001f;
    float3 lightV = plInfo.position - worldOrigin;
    float3 lightDir = normalize(lightV);
    float distToLight = length(lightV);

    float ambient = 0.3;
    float factor = IsShadowed(lightDir, worldOrigin, distToLight, vertex.normal) ? ambient : 1.0;

    float3 color = factor * diffuse.rgb * max(dot(lightDir, vertex.normal), ambient) * plInfo.color * plInfo.luminocity / pow(distToLight, 2);

    //TODO: Maybe avoidable to always do this loop if no transparency was encountered. Minor improvement expected though...
    [unroll]
    for (int i = 0; i < K; i++)
        color += (1 - payload.node[i].transmit) * payload.node[i].color;

    payload.ShadedColorAndHitT = float4(color, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(lod, lod, lod) / 9, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(fovealDist, fovealDist, fovealDist), RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(a, a, a) * 200, RayTCurrent());
}