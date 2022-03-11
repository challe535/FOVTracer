#include "Common.hlsl"
#include "KernelFov.hlsl"

void fillOutputAtIndex(float2 index, float4 color, float maxDist, RWTexture2D<float4> dest)
{
    float foveaDist = length(index - params.fovealCenter) / maxDist;
    float indexMultiplier = params.viewportRatio;

    float2 upperIndex = round(index.xy * indexMultiplier + params.foveationFillOffset * (params.isFoveatedRenderingEnabled ? kernelFunc(foveaDist, params.kernelAlpha) : 1));
    float2 lowerIndex = round(max(index.xy - 1, 0) * indexMultiplier);

    for (float x = lowerIndex.x; x < upperIndex.x; x++)
    {
        for (float y = lowerIndex.y; y < upperIndex.y; y++)
        {
            dest[float2(x, y)] = color;
        }
    }
}

[shader("raygeneration")]
void RayGen()
{
    float2 LaunchIndex = float2(DispatchRaysIndex().xy);
    float2 LaunchDimensions = float2(DispatchRaysDimensions().xy);
    
    float aspectRatio = (LaunchDimensions.x / LaunchDimensions.y);
	
    float2 fovealPoint = params.fovealCenter / params.viewportRatio;

    float2 l1 = LaunchDimensions - fovealPoint;
    float2 l2 = float2(l1.x, 0 - fovealPoint.y);
    float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
    float2 l4 = float2(0 - fovealPoint.x, l1.y);

    float maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
    float L = log(maxCornerDist);
    float B = 2 * PI / LaunchDimensions.y;

    float3 finalColor = float3(0, 0, 0);
    float2 finalMotion = float2(0, 0);
    float4 finalWorldPosAndDepth = float4(0, 0, 0, 0);

    float3 worldDelta = WorldPosBuffer[LaunchIndex.xy].xyz - viewOriginAndTanHalfFovY.xyz;
    float4 clip = mul(view, float4(worldDelta, 0));

    clip.x /= aspectRatio;
    clip.xy /= viewOriginAndTanHalfFovY.w * clip.z;
    clip.y = -clip.y;

    clip.xy = clip.xy * 0.5 + 0.5;

    float2 motion = (LaunchIndex - clip.xy * LaunchDimensions) + 0.5;

    //float far = 1000.f;
    //float near = 0.1f;
    //float fnFactor = -far / (far - near);
    //float4 clip = float4(float2(viewPos.xy) / viewOriginAndTanHalfFovY.w, viewPos.z * fnFactor - viewPos.w, viewPos.w * fnFactor * near);

	//super sampling
    float pixelPadding = 0.1;

    float stepSize = 1.0 / float(params.sqrtSamplesPerPixel + 1);

    float offsetX = stepSize;
    float offsetY = stepSize;

    for (int i = 0; i < params.sqrtSamplesPerPixel; i++)
    {
        offsetY = stepSize;
        for (int j = 0; j < params.sqrtSamplesPerPixel; j++)
        {
            float2 AdjustedIndex = LaunchIndex * (!params.isFoveatedRenderingEnabled ? params.viewportRatio : 1.0) + float2(offsetX, offsetY);
            float2 d = ((AdjustedIndex / LaunchDimensions.xy) * 2.f - 1.f);
            if (params.isFoveatedRenderingEnabled)
            {
                float2 logPolar2Screen = exp(L * kernelFunc(AdjustedIndex.x / LaunchDimensions.x, params.kernelAlpha)) * float2(cos(B * AdjustedIndex.y), sin(B * AdjustedIndex.y)) + fovealPoint;
            
                d = ((float2(logPolar2Screen.x, logPolar2Screen.y) / LaunchDimensions.xy) * 2.f - 1.f);
            }
            float3 vToNear = (d.x * view[0].xyz * viewOriginAndTanHalfFovY.w * aspectRatio) - (d.y * view[1].xyz * viewOriginAndTanHalfFovY.w) + view[2].xyz;

	        // Setup the ray
            RayDesc ray;
            ray.Origin = viewOriginAndTanHalfFovY.xyz;
            ray.Direction = normalize(vToNear);
            ray.TMin = 0.1f;
            ray.TMax = 100000.f;

	        // Trace the ray
            HitInfo payload;
            payload.ShadedColorAndHitT = float4(0.f, 0.f, 0.f, 0.f);

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
            finalWorldPosAndDepth.rgb += payload.ShadedColorAndHitT.w * ray.Direction + ray.Origin;
            finalWorldPosAndDepth.a += payload.ShadedColorAndHitT.w * 0.0005;

            offsetY += stepSize;
        }
        offsetX += stepSize;
    }

    float avgFactor = 1 / pow(float(params.sqrtSamplesPerPixel), 2);

    finalColor *= avgFactor;
    //finalMotion *= avgFactor;
    finalWorldPosAndDepth *= avgFactor;

    finalWorldPosAndDepth.a = clamp(finalWorldPosAndDepth.a, 0, 1);

    if (params.isFoveatedRenderingEnabled)
    {
        RTOutput[LaunchIndex.xy] = float4(finalColor, 1.0f);
        //MotionOutput[LaunchIndex.xy] = float4(finalMotion, 0, 0);
    }
    else
    {
        fillOutputAtIndex(LaunchIndex.xy, float4(finalColor, 1.0f), maxCornerDist, RTOutput);
        //fillOutputAtIndex(LaunchIndex.xy, float4(finalMotion, 0, 0), maxCornerDist, MotionOutput);
    }

    MotionOutput[LaunchIndex.xy] = float4(motion, 0, 0);
    WorldPosBuffer[LaunchIndex.xy] = finalWorldPosAndDepth;
    
}
