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
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE,
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
    ray.TMax = params.rayTMax;

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
		    RAY_FLAG_FORCE_OPAQUE,
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
    return normalize(mapping.x * vertex.tangent + mapping.y * vertex.binormal + mapping.z * vertex.normal);
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

    int2 coord = floor(frac(vertex.uv) * material.textureResolution.xy);

    
    //Normals
    if (material.hasNormalMap)
    {
        vertex.normal = CalcMappedNormal(vertex, coord);
    }
    
    if (params.flipNormals)
        vertex.normal = -vertex.normal;
    
    // --- Light ---
    PointLightInfo plInfo;

    plInfo.position = float3(-200, 700, 0);
    plInfo.color = float3(1, 1, 1.0);
    plInfo.luminocity = 500000;
    //plInfo.luminocity = 2.5;
    
    float3 viewDirection = normalize(WorldRayOrigin() - vertex.position);

    float3 worldOriginOffsetOut = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.01f;
    float3 worldOriginOffsetIn = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() - vertex.normal * 0.01f;
    float3 lightV = plInfo.position - worldOriginOffsetOut;
    float3 lightDir = normalize(lightV);
    float distToLight = length(lightV);

    //lightDir = normalize(float3(1, 0, 0));
    //distToLight = params.rayTMax;
    
    //Shadow calc
    bool shadowed = /*payload.RecursionDepthRemaining == params.recursionDepth ? */IsShadowed(lightDir, worldOriginOffsetOut, distToLight, vertex.normal) /* : false*/;
    float3 lightColor = plInfo.color * plInfo.luminocity / pow(distToLight, 2);
    
    //Texture LOD
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

    
    float3 color = material.AmbientColor * 0.05;

    float effectThreshold = 0.25;
    float effectMargin = 0.1;
    float distanceNormalizer = 1 / params.rayTMax;
    float normedDist = RayTCurrent() * distanceNormalizer;
    float dropoffT = normedDist - (effectThreshold - effectMargin);
    float effectDropoff = 1 - (1/effectMargin) * max(dropoffT, 0);
    
    if (shadowed)
    {
        color *= diffuse;
    }
    else
    {
        color += diffuse * max(dot(lightDir, vertex.normal), 0.0f) * lightColor;
        color += material.SpecularColor * pow(max(dot(normalize(reflect(-lightDir, vertex.normal)), viewDirection), 0.0), material.Shininess);
    }
    
    //float reflectivity = (material.SpecularColor.r + material.SpecularColor.g + material.SpecularColor.b) / 3.0;
    float reflectivity = pow((material.RefractIndex - 1) / (material.RefractIndex + 1), 2);
    if (reflectivity > 0 && payload.RecursionDepthRemaining > 0 && normedDist < effectThreshold)
    {
        float3 reflectDirection = normalize(reflect(-viewDirection, vertex.normal));
        color += reflectivity * LaunchRecursive(reflectDirection, payload.RecursionDepthRemaining, worldOriginOffsetOut).rgb * effectDropoff;
    }
    
    if (any(material.TransmitanceFilter < 0.999) && payload.RecursionDepthRemaining > 0 && normedDist < effectThreshold)
    {
        bool isBackFacing = dot(WorldRayDirection(), vertex.normal) > 0;
        float3 normal = isBackFacing ? -vertex.normal : vertex.normal;
        
        float refractI = material.RefractIndex;
        refractI = isBackFacing ? refractI : 1.0 / refractI;
        float3 refractDirection = normalize(refract(WorldRayDirection(), normal, refractI));
       
        float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() - normal * 0.001;
        
        if (length(refractDirection) > 0)
            color += (1 - material.TransmitanceFilter) * LaunchRecursive(refractDirection, payload.RecursionDepthRemaining, worldPos).rgb * effectDropoff;
    }
    
    //Random Global Illum
    //if (payload.RecursionDepthRemaining > 0)
        if (payload.RecursionDepthRemaining == params.recursionDepth && params.useIndirectIllum && normedDist < effectThreshold)
        {
            float3 outDir = RandOnUnitSphere(float4(worldOriginOffsetOut, params.elapsedTimeSeconds % 20));

            outDir *= sign(dot(outDir, vertex.normal));
        
            float4 illumColorAndT = LaunchRecursive(outDir, payload.RecursionDepthRemaining, worldOriginOffsetOut);
            color += illumColorAndT.rgb * max(dot(outDir, vertex.normal), 0.0f) * effectDropoff;
        }

    //TODO: Maybe avoidable to always do this loop if no transparency was encountered. Minor improvement expected though...
    [unroll]
        for (int i = 0; i < K; i++)
            color += (1 - payload.node[i].transmit) * payload.node[i].color;

        payload.ShadedColorAndHitT = float4(color, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(vertex.normal, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(RayTCurrent(), RayTCurrent(), RayTCurrent(), RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(lod, lod, lod) / 9, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(fovealDist, fovealDist, fovealDist), RayTCurrent());
    //payload.ShadedColorAndHitT = float4(float3(a, a, a) * 200, RayTCurrent());
    }