#include "Common.hlsl"
#include "KernelFov.hlsl"

float Rand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
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

float2 LogPolar2Screen(float2 logIndex, float2 dimensions, float2 fovealPoint, float B, float L)
{
    return exp(L * kernelFunc(logIndex.x / dimensions.x, params.kernelAlpha)) * float2(cos(B * logIndex.y), sin(B * logIndex.y)) + fovealPoint;
}

float3 GetRayDir(float2 index, float2 dimensions, float aspectRatio, float2 fovealPoint, float B, float L)
{
    float2 d = ((index / dimensions.xy) * 2.f - 1.f);
    if (params.isFoveatedRenderingEnabled)
    {
        float2 logPolar2Screen = LogPolar2Screen(index, dimensions, fovealPoint, B, L);
            
        d = ((float2(logPolar2Screen.x, logPolar2Screen.y) / dimensions.xy) * 2.f - 1.f);
    }
    float3 dir = (d.x * view[0].xyz * viewOriginAndTanHalfFovY.w * aspectRatio) - (d.y * view[1].xyz * viewOriginAndTanHalfFovY.w) + view[2].xyz;

    return normalize(dir);

}

[shader("raygeneration")]
void RayGen()
{
    float2 LaunchIndex = float2(DispatchRaysIndex().xy);
    float2 LaunchDimensions = float2(DispatchRaysDimensions().xy);
    
    float aspectRatio = (LaunchDimensions.x / LaunchDimensions.y);
	
    float2 fovealPoint = params.fovealCenter * LaunchDimensions;

    float2 l1 = LaunchDimensions - fovealPoint;
    float2 l2 = float2(l1.x, 0 - fovealPoint.y);
    float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
    float2 l4 = float2(0 - fovealPoint.x, l1.y);

    float maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
    float L = log(maxCornerDist);
    float B = 2 * PI / LaunchDimensions.y;

	//super sampling
    float3 finalColor = float3(0, 0, 0);
    float4 finalWorldPosAndDepth = float4(0, 0, 0, 0);

    float stepSize = 1.0 / float(params.sqrtSamplesPerPixel + 1);
    
    float offsetX = stepSize;
    float offsetY = stepSize;

    float2 jitter = jitterOffset /** (params.isFoveatedRenderingEnabled ? 0 : 1)*/;

    for (int i = 0; i < params.sqrtSamplesPerPixel; i++)
    {
        offsetY = stepSize;
        for (int j = 0; j < params.sqrtSamplesPerPixel; j++)
        {
            float2 AdjustedIndex = LaunchIndex + float2(offsetX, offsetY);
            float2 JitteredIndex = AdjustedIndex + jitter;

            float3 rayDir = GetRayDir(AdjustedIndex, LaunchDimensions, aspectRatio, fovealPoint, B, L);
            float3 jitterDir = GetRayDir(JitteredIndex, LaunchDimensions, aspectRatio, fovealPoint, B, L);

	        // Setup the ray
            RayDesc ray;
            ray.Origin = viewOriginAndTanHalfFovY.xyz;
            ray.Direction = jitterDir;
            ray.TMin = 0.01f;
            ray.TMax = 100000.f;

	        // Trace the ray
            HitInfo payload;
            payload.ShadedColorAndHitT = float4(0.f, 0.f, 0.f, 0.f);
            payload.RecursionDepthRemaining = 3;

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

            finalColor += payload.ShadedColorAndHitT.rgb;
            finalWorldPosAndDepth.rgb += payload.ShadedColorAndHitT.w * rayDir + ray.Origin;
            finalWorldPosAndDepth.a += payload.ShadedColorAndHitT.w;

            offsetY += stepSize;
        }
        offsetX += stepSize;
    }

    float avgFactor = 1 / pow(float(params.sqrtSamplesPerPixel), 2);

    finalColor *= avgFactor;
    finalWorldPosAndDepth *= avgFactor;

    finalWorldPosAndDepth.a = clamp(finalWorldPosAndDepth.a * 0.0005, 0, 1);


    RTOutput[LaunchIndex.xy] = float4(finalColor, 1.0f);
    
    //switch (params.outBufferIndex)
    //{
    //    case 0:
    //        RTOutput[LaunchIndex.xy] = float4(finalColor, 1.0f);
    //        break;
    //    case 1:
    //        RTOutput1[LaunchIndex.xy] = float4(finalColor, 1.0f);
    //        break;
    //    case 2:
    //        RTOutput2[LaunchIndex.xy] = float4(finalColor, 1.0f);
    //        break;
    //    case 3:
    //        RTOutput3[LaunchIndex.xy] = float4(finalColor, 1.0f);
    //        break;
    //    case 4:
    //        RTOutput4[LaunchIndex.xy] = float4(finalColor, 1.0f);
    //        break;
    //    default:
    //        RTOutput[LaunchIndex.xy] = float4(0, 0, 0, 1.0f);
    //        RTOutput1[LaunchIndex.xy] = float4(0, 0, 0, 1.0f);
    //        RTOutput2[LaunchIndex.xy] = float4(0, 0, 0, 1.0f);
    //        RTOutput3[LaunchIndex.xy] = float4(0, 0, 0, 1.0f);
    //        RTOutput4[LaunchIndex.xy] = float4(0, 0, 0, 1.0f);
    //        break;
    //}
    
    float2 motionIndex = params.isFoveatedRenderingEnabled ? LogPolar2Screen(LaunchIndex + 0.5, LaunchDimensions, fovealPoint, B, L) : LaunchIndex + 0.5;
    float2 motion = motionIndex - getClip(WorldPosBuffer[LaunchIndex].xyz, aspectRatio).xy * LaunchDimensions;
    MotionOutput[LaunchIndex.xy] = motion;

    WorldPosBuffer[LaunchIndex.xy] = finalWorldPosAndDepth;
}
