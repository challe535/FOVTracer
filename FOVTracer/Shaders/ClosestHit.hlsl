#include "Common.hlsl"
#include "KernelFov.hlsl"

//Credit: https://blog.demofox.org/2020/05/16/using-blue-noise-for-raytraced-soft-shadows/
static float2 BlueNoiseInDisk[64] =
{
    float2(0.478712, 0.875764),
    float2(-0.337956, -0.793959),
    float2(-0.955259, -0.028164),
    float2(0.864527, 0.325689),
    float2(0.209342, -0.395657),
    float2(-0.106779, 0.672585),
    float2(0.156213, 0.235113),
    float2(-0.413644, -0.082856),
    float2(-0.415667, 0.323909),
    float2(0.141896, -0.939980),
    float2(0.954932, -0.182516),
    float2(-0.766184, 0.410799),
    float2(-0.434912, -0.458845),
    float2(0.415242, -0.078724),
    float2(0.728335, -0.491777),
    float2(-0.058086, -0.066401),
    float2(0.202990, 0.686837),
    float2(-0.808362, -0.556402),
    float2(0.507386, -0.640839),
    float2(-0.723494, -0.229240),
    float2(0.489740, 0.317826),
    float2(-0.622663, 0.765301),
    float2(-0.010640, 0.929347),
    float2(0.663146, 0.647618),
    float2(-0.096674, -0.413835),
    float2(0.525945, -0.321063),
    float2(-0.122533, 0.366019),
    float2(0.195235, -0.687983),
    float2(-0.563203, 0.098748),
    float2(0.418563, 0.561335),
    float2(-0.378595, 0.800367),
    float2(0.826922, 0.001024),
    float2(-0.085372, -0.766651),
    float2(-0.921920, 0.183673),
    float2(-0.590008, -0.721799),
    float2(0.167751, -0.164393),
    float2(0.032961, -0.562530),
    float2(0.632900, -0.107059),
    float2(-0.464080, 0.569669),
    float2(-0.173676, -0.958758),
    float2(-0.242648, -0.234303),
    float2(-0.275362, 0.157163),
    float2(0.382295, -0.795131),
    float2(0.562955, 0.115562),
    float2(0.190586, 0.470121),
    float2(0.770764, -0.297576),
    float2(0.237281, 0.931050),
    float2(-0.666642, -0.455871),
    float2(-0.905649, -0.298379),
    float2(0.339520, 0.157829),
    float2(0.701438, -0.704100),
    float2(-0.062758, 0.160346),
    float2(-0.220674, 0.957141),
    float2(0.642692, 0.432706),
    float2(-0.773390, -0.015272),
    float2(-0.671467, 0.246880),
    float2(0.158051, 0.062859),
    float2(0.806009, 0.527232),
    float2(-0.057620, -0.247071),
    float2(0.333436, -0.516710),
    float2(-0.550658, -0.315773),
    float2(-0.652078, 0.589846),
    float2(0.008818, 0.530556),
    float2(-0.210004, 0.519896)
};

struct OrbLightInfo
{
    float3 position;
    float3 color;
    float luminocity;
    float radius;
};

float Rand(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

float3 getConeSample(uint sampleNum, float3 vToLightCenter, float radius, out float distToLightEdge, uint totalSamples)
{
    float3 perpL = normalize(cross(vToLightCenter, float3(0.f, 1.0f, 0.f)));
    float3 perpU = normalize(cross(perpL, vToLightCenter));
    
    float2 offset = 0;

    float frameCount = params.frameCount % 128;
    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float2 posSeed = float2(worldPos.x * worldPos.y / (worldPos.z * GOLDEN), worldPos.x * worldPos.y + worldPos.z * PI);
    float angle = frac(blueNoise.Load(int3((DispatchRaysIndex().xy + frac(posSeed) * 128) % 128, 0)).x + frameCount * GOLDEN) * 2 * PI;
    float2x2 rot = { cos(angle), -sin(angle), sin(angle), cos(angle) };
    
    float sampleRand = frac(sin(frac(posSeed.x * posSeed.y + frameCount * GOLDEN) * 2 * PI));
    offset = mul(rot, BlueNoiseInDisk[(floor(sampleRand * (64 - totalSamples)) + sampleNum) % 64]) * radius;
    
    float3 vInDisk = perpL * offset.x + perpU * offset.y;
    
    float3 shadowVector = vToLightCenter + vInDisk;
    distToLightEdge = length(shadowVector);
    return normalize(shadowVector);
}

float3 getConeSampleFromLightInfo(uint sampleNum, OrbLightInfo light, float3 origin, out float distToLightEdge, uint totalSamples)
{
    float3 lightV = light.position - origin;
    return getConeSample(sampleNum, lightV, light.radius, distToLightEdge, totalSamples);
}

float3 RandOnUnitSphere(float4 seed)
{
    float theta = Rand(frac(seed.xy + seed.zw)) * 2 * PI;
    float v = Rand(frac(seed.yw * seed.xz));
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

float3 sampleOrbLight(uint sampleN, OrbLightInfo lInfo, float3 origin, float3 normal, float3 viewDir, float3 diffuse)
{
    float3 contribution = 0;
    uint shadowSamples = sampleN;
    float brightness = 0;
    for (int i = 0; i < shadowSamples; i++)
    {
        float distToLight = params.rayTMax;
        float3 lightDir = getConeSampleFromLightInfo(i, lInfo, origin, distToLight, shadowSamples);
        float lightBrightness = lInfo.luminocity / pow(distToLight, 2);
        bool shadowed = lightBrightness > 0.001 ? IsShadowed(lightDir, origin, distToLight, normal) : false;
        
        if (!shadowed)
        {
            contribution += diffuse * max(dot(lightDir, normal), 0.0f);
            contribution += material.SpecularColor * pow(max(dot(normalize(reflect(-lightDir, normal)), viewDir), 0.0), material.Shininess);
            brightness += lightBrightness;
        }
    }
    contribution /= shadowSamples;
    contribution *= lInfo.color * brightness / shadowSamples;
    return contribution;
}

float3 sampleDirLight(uint sampleN, float3 dir, float3 origin, float3 normal, float3 viewDir, float3 diffuse, float radius, float strength)
{
    uint samples = sampleN;
    float3 contribution = 0;
    float dist = 0;
    for (int i = 0; i < samples; i++)
    {
        float3 lightDir = getConeSample(i, dir, radius, dist, samples);
        if (!IsShadowed(lightDir, origin, params.rayTMax, normal))
        {
            contribution += diffuse * max(dot(lightDir, normal), 0.0f);
            contribution += material.SpecularColor * pow(max(dot(normalize(reflect(-lightDir, normal)), viewDir), 0.0), material.Shininess);
        }
    }
    return strength * contribution / samples;
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

    float3 color = diffuse * material.AmbientColor;
    
    // --- Light ---
    OrbLightInfo plInfo;

    //Sponza
    plInfo.position = float3(-200, 700, 0);
    plInfo.color = float3(1, 1, 1.0);
    plInfo.luminocity = 250000;
    plInfo.radius = 50;
    
    //Cornell-box
    //plInfo.position = float3(0, 1.39, 0);
    //plInfo.color = float3(1, 1, 1.0);
    //plInfo.luminocity = 1;
    //plInfo.radius = 0.1;
    
    //Sibenik
    //plInfo.position = float3(6.5, -4, 0);
    //plInfo.color = float3(1, 1, 1);
    //plInfo.luminocity = 60;
    //plInfo.radius = 1;
    
    //OrbLightInfo plInfo2;
    //plInfo2.position = float3(-10, -6, 0);
    //plInfo2.color = float3(0.8, 0.8, 1);
    //plInfo2.luminocity = 30;
    //plInfo2.radius = 1;
    
    //Sun temple
    //plInfo.position = float3(-4, 242, -3176);
    //plInfo.color = float3(.988, .541, .09);
    //plInfo.luminocity = 30000;
    //plInfo.radius = 50;
    
    //OrbLightInfo plInfo2;
    //plInfo2.position = float3(-60, 905, -2520);
    //plInfo2.color = float3(0.8, 0.8, 1);
    //plInfo2.luminocity = 600000;
    //plInfo2.radius = 50;
    
    //Stanford bunny
    //plInfo.position = float3(0.3, 1.5, -0.3);
    //plInfo.color = float3(1, 1, 1.0);
    //plInfo.luminocity = 1;
    //plInfo.radius = 0.0;
    
    float3 viewDirection = normalize(WorldRayOrigin() - vertex.position);

    float3 worldOriginOffsetOut = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.01f;
    float3 worldOriginOffsetIn = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() - vertex.normal * 0.01f;
    
    //Shadow calc
    float distToLight = params.rayTMax;
    float3 lightDir = 0;
    bool shadowed = false;
    
    float3 skyBlue = float3(0.529, 0.808, 0.922);
    
    //Default ambiance;
    color *= 0.2;

    
    //Sun temple ambiance
    //color *= 0.75;
    
    color += skyBlue * 0.1;
    float3 ambientBaseline = color;
    
    color += sampleOrbLight(3, plInfo, worldOriginOffsetOut, vertex.normal, viewDirection, diffuse);
    //color += sampleOrbLight(3, plInfo2, worldOriginOffsetOut, vertex.normal, viewDirection, diffuse);
    
    //Directional light
    
    //Sun Temple
    //float3 sunDir = float3(0, 0.3, 1);
    //float sunRadiusParam = 0.02;
    //float sunStrength = 1;
    
    //Sponza
    float3 sunDir = float3(0, 0.9, 1);
    float sunRadiusParam = 0.6;
    float sunStrength = 1;
    
    color += sampleDirLight(3, sunDir, worldOriginOffsetOut, vertex.normal, viewDirection, diffuse, sunRadiusParam, sunStrength);

    float effectThreshold = 0.35;
    float effectMargin = 0.1;
    float distanceNormalizer = 1 / params.rayTMax;
    float normedDist = RayTCurrent() * distanceNormalizer;
    float dropoffT = normedDist - (effectThreshold - effectMargin);
    float effectDropoff = 1 - (1/effectMargin) * max(dropoffT, 0);
    
    //There doesn't seem to be any clear cut way to determine reflectivity so adjust on a scene by scene basis.
    float reflectivity = 0;
    //Use this for Cornell-Box
    //if (any(material.TransmitanceFilter < 0.999))
    //    reflectivity = pow((material.RefractIndex - 1) / (material.RefractIndex + 1), 2);
    //else
    //    reflectivity = (material.SpecularColor.r + material.SpecularColor.g + material.SpecularColor.b) / 3.0;

    //Use this for sponza
    reflectivity = pow((material.RefractIndex - 1) / (material.RefractIndex + 1), 2);
    
    //Use this for sibenik
    //reflectivity = (material.SpecularColor.r + material.SpecularColor.g + material.SpecularColor.b) / 3.0;
    
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
        float2 blueNoiseVec = BlueNoiseInDisk[params.frameCount % 64];
        float worldSeed = frac(worldOriginOffsetOut.x * worldOriginOffsetOut.y + worldOriginOffsetOut.z);
        float3 outDir = RandOnUnitSphere(float4(frac(params.frameCount * GOLDEN), worldSeed, blueNoiseVec.x + blueNoiseVec.y, params.elapsedTimeSeconds % 20));

        outDir *= sign(dot(outDir, vertex.normal));
        
        float4 illumColorAndT = LaunchRecursive(outDir, payload.RecursionDepthRemaining, worldOriginOffsetOut);
        color += clamp(illumColorAndT.rgb * max(dot(outDir, vertex.normal), 0.0f) * effectDropoff * pow(1 - clamp(illumColorAndT.a / params.rayTMax, 0, 1), 2) - ambientBaseline, 0, 1);
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