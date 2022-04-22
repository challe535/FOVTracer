#include "Common.hlsl"
#include "KernelFov.hlsl"

struct PointLightInfo
{
    float3 position;
    float3 color;
    float luminocity;
};

float Rand(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

float3 RandOnUnitSphere(float4 seed)
{
    float l0 = length(seed);
    float l1 = l0 * l0;
    float theta = Rand(float2(l0, l0 * 0.5)) * 2 * PI;
    float v = Rand(float2(l1, l1 * 0.5));
    float phi = acos((2 * v) - 1);
    float3 pt = float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));

    return pt;
}

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
    shadowPayload.isHit = false;

    TraceRay(
		SceneBVH,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
		0xFF,
		0,
		0,
		0,
		ray,
		shadowPayload
	);
    
    return shadowPayload.isHit;
}

float4 LaunchRecursive(float3 dir, uint currentDepth, float3 origin)
{
    // Setup the ray
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.01f;
    ray.TMax = 100000.f;

	// Trace the ray
    HitInfo payload;
    payload.ShadedColorAndHitT = float4(0.f, 0.f, 0.f, 0.f);
    payload.RecursionDepthRemaining = currentDepth - 1;

    Node n;
    n.depth = ray.TMax * 2;
    n.transmit = 1;
    n.color = float3(0, 0, 0);

        [unroll]
    for (int k = 0; k < K; k++)
        payload.node[k] = n;
                

    TraceRay(
		    SceneBVH,
		    RAY_FLAG_NONE,
		    0xFF,
		    1,
		    1,
		    1,
		    ray,
		    payload
            );
    
    return payload.ShadedColorAndHitT;
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
    float lodBias = 0.35;

    if (params.isFoveatedRenderingEnabled)
        lodBias += 3 * pow(smoothstep(0, 1, kernelFunc(indexNorm.x, params.kernelAlpha)), 3);

    if (params.isDLSSEnabled && !params.isFoveatedRenderingEnabled)
        lodBias += log2(DispatchRaysDimensions().x / displayResolution.x) - 1.0f + 0.0001;

    float lod = clamp(lodBias + log2(coneFactor), 0, 9);

    float3 diffuse = material.DiffuseColor;
    if (material.hasDiffuseTexture)
    {
        diffuse *= albedo.SampleLevel(BilinearClamp, vertex.uv, lod).rgb;
    }

    if (material.hasNormalMap)
    {
        vertex.normal = CalcMappedNormal(vertex, coord);
    }

    // --- Light ---
    PointLightInfo plInfo;

    plInfo.position = float3(0, 1.5 /*+ 100 * sin(params.elapsedTimeSeconds)*/, 0);
    plInfo.color = float3(0.9, 0.9, 1.0);
    plInfo.luminocity = 1.0;
    
    float3 viewDirection = normalize(viewOriginAndTanHalfFovY.xyz - vertex.position);

    float3 worldOriginOffsetOut = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.001f;
    float3 worldOriginOffsetIn = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() - vertex.normal * 0.001f;
    float3 lightV = plInfo.position - worldOriginOffsetOut;
    float3 lightDir = normalize(lightV);
    float distToLight = length(lightV);

    //float3 ambient = 0.3;
    //float factor =  ? ambient : 1.0;
    
    float3 color = material.AmbientColor * 0.05;
    //float3 color = 0;

    float reflectivity = (material.SpecularColor.r + material.SpecularColor.g + material.SpecularColor.b) / 3.0;
    
    if (!IsShadowed(lightDir, worldOriginOffsetOut, distToLight, vertex.normal))
    {
        color += diffuse * max(dot(lightDir, vertex.normal), 0.0f) * plInfo.color * plInfo.luminocity / pow(distToLight, 2);
        color += material.SpecularColor * pow(max(dot(normalize(reflect(-lightDir, vertex.normal)), viewDirection), 0.0), material.Shininess);
    }
    
    if (reflectivity > 0 && payload.RecursionDepthRemaining > 0)
    {
        float3 reflectDirection = normalize(reflect(-viewDirection, vertex.normal));
        color += reflectivity * LaunchRecursive(reflectDirection, payload.RecursionDepthRemaining, worldOriginOffsetOut).rgb;
    }
    
    if (material.RefractIndex > 1 && payload.RecursionDepthRemaining > 0)
    {
        float normalAlignment = sign(dot(WorldRayDirection(), vertex.normal));
        float refractI = normalAlignment > 0 ? material.RefractIndex : 1 / material.RefractIndex;
        
        float3 refractDirection = normalize(refract(WorldRayDirection(), vertex.normal * -normalAlignment, refractI));
        if (length(refractDirection) > 0)
            color += (1 - material.TransmitanceFilter) * LaunchRecursive(refractDirection, payload.RecursionDepthRemaining, worldOriginOffsetIn).rgb;
    }
    
    //Random Global Illum
    if (payload.RecursionDepthRemaining > 0)
    {
        float3 outDir = RandOnUnitSphere(float4(worldOriginOffsetOut, params.elapsedTimeSeconds % 20));

        outDir *= sign(dot(outDir, vertex.normal));
        
        float4 illumColorAndT = LaunchRecursive(outDir, payload.RecursionDepthRemaining, worldOriginOffsetOut);
        color += illumColorAndT.rgb * max(dot(outDir, vertex.normal), 0.0f) /*/ pow(illumColorAndT.a, 2)*/;
    }
    

    //TODO: Maybe avoidable to always do this loop if no transparency was encountered. Minor improvement expected though...
    [unroll]
        for (int i = 0; i < K; i++)
            color += (1 - payload.node[i].transmit) * payload.node[i].color;

    payload.ShadedColorAndHitT = float4(color, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(RayTCurrent(), RayTCurrent(), RayTCurrent(), RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(lod, lod, lod) / 9, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(fovealDist, fovealDist, fovealDist), RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(a, a, a) * 200, RayTCurrent());
}