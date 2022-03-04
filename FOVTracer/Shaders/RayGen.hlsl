#include "Common.hlsl"

//Quite unstable for now. Remember to stop clearing output in command list when using.
#define STOCH 0

float random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

[shader("raygeneration")]
void RayGen()
{
    float2 LaunchIndex = DispatchRaysIndex().xy;
    float2 LaunchDimensions = float2(DispatchRaysDimensions().xy);
	
    float2 logPolarSize = LaunchDimensions * 1.0f;
    float2 fovealPoint = fovealCenter;
    float kernelAlpha = 1.0f;

    float2 l1 = LaunchDimensions - fovealPoint;
    float2 l2 = float2(l1.x, 0 - fovealPoint.y);
    float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
    float2 l4 = float2(0 - fovealPoint.x, l1.y);

    float L = log(max(max(length(l1), length(l2)), max(length(l3), length(l4))));
    float A = L / logPolarSize.x;
    float B = 2 * PI / logPolarSize.y;

#if STOCH
    float2 relativeIndex = LaunchIndex - fovealPoint;
    float logPolarU = 1.0 - pow(log(length(relativeIndex)) / L, 1.0 / kernelAlpha);
    float test = random(float2(elapsedTimeSeconds, elapsedTimeSeconds * elapsedTimeSeconds));
    float rndOffset = random(LaunchIndex) * 0.1 * length(relativeIndex) / exp(L);
    if (test > logPolarU + rndOffset)
        return;
#else
    float2 logPolarOutCoord = float2(
        exp(A * pow(LaunchIndex.x, kernelAlpha)) * cos(B * LaunchIndex.y) + fovealPoint.x,
        exp(A * pow(LaunchIndex.x, kernelAlpha)) * sin(B * LaunchIndex.y) + fovealPoint.y);
#endif

        float3 finalColor = float3(0, 0, 0);

	//super sampling
    float pixelPadding = 0.1;

    float stepSize = 1.0 / float(sqrtSamplesPerPixel + 1);

    float offsetX = stepSize;
    float offsetY = stepSize;

    for (int i = 0; i < sqrtSamplesPerPixel; i++)
    {
        offsetY = stepSize;
        for (int j = 0; j < sqrtSamplesPerPixel; j++)
        {
            float2 AdjustedIndex = LaunchIndex + float2(offsetX, offsetY);
#if STOCH
            float2 d = ((AdjustedIndex / LaunchDimensions.xy) * 2.f - 1.f);
#else
            float2 logPolar2Screen = float2(
                exp(A * pow(AdjustedIndex.x, kernelAlpha)) * cos(B * AdjustedIndex.y) + fovealPoint.x,
                exp(A * pow(AdjustedIndex.x, kernelAlpha)) * sin(B * AdjustedIndex.y) + fovealPoint.y);
            
            float2 d = ((float2(logPolar2Screen.x, logPolar2Screen.y) / LaunchDimensions.xy) * 2.f - 1.f);
#endif
            float aspectRatio = (LaunchDimensions.x / LaunchDimensions.y);

	        // Setup the ray
            RayDesc ray;
            ray.Origin = viewOriginAndTanHalfFovY.xyz;
            ray.Direction = normalize((d.x * view[0].xyz * viewOriginAndTanHalfFovY.w * aspectRatio) - (d.y * view[1].xyz * viewOriginAndTanHalfFovY.w) + view[2].xyz);
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

            offsetY += stepSize;
        }
        offsetX += stepSize;
    }

    finalColor /= pow(float(sqrtSamplesPerPixel),2);

#if STOCH
     RTOutput[LaunchIndex.xy] = float4(finalColor, 1.0f);
#else
    float4 RTColor = RTOutput[logPolarOutCoord.xy];

    RTColor.rgb *= RTColor.a;
    RTColor.rgb += finalColor;
    RTColor.rgb /= RTColor.a + 1.0f;

    RTOutput[logPolarOutCoord.xy] = float4(RTColor.rgb, RTColor.a + 1.0f);
#endif
   
}
