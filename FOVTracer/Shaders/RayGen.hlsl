#include "Common.hlsl"

float kernelFunc(float x)
{
    return pow(x, params.kernelAlpha);
}

void fillOutputAtIndex(float2 index, float4 color, float maxDist)
{
    float foveaDist = length(index - params.fovealCenter) / maxDist;
    float indexMultiplier = params.viewportRatio;

    float2 upperIndex = round(index.xy * indexMultiplier + params.foveationFillOffset * (params.isFoveatedRenderingEnabled ? kernelFunc(foveaDist) : 1));
    float2 lowerIndex = round(max(index.xy - 1, 0) * indexMultiplier);

    for (float x = lowerIndex.x; x < upperIndex.x; x++)
    {
        for (float y = lowerIndex.y; y < upperIndex.y; y++)
        {
            RTOutput[float2(x, y)] = color;  
        }
    }
}

[shader("raygeneration")]
void RayGen()
{
    float2 LaunchIndex = float2(DispatchRaysIndex().xy);
    float2 LaunchDimensions = float2(DispatchRaysDimensions().xy);
	
    float2 fovealPoint = params.fovealCenter / params.viewportRatio;

    float2 l1 = LaunchDimensions - fovealPoint;
    float2 l2 = float2(l1.x, 0 - fovealPoint.y);
    float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
    float2 l4 = float2(0 - fovealPoint.x, l1.y);

    float maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
    float L = log(maxCornerDist);
    float B = 2 * PI / LaunchDimensions.y;

    float3 finalColor = float3(0, 0, 0);

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
            float2 AdjustedIndex = LaunchIndex + float2(offsetX, offsetY);
            float2 d = ((AdjustedIndex / LaunchDimensions.xy) * 2.f - 1.f);
            if (params.isFoveatedRenderingEnabled)
            {
                float2 logPolar2Screen = exp(L * kernelFunc(AdjustedIndex.x / LaunchDimensions.x)) * float2(cos(B * AdjustedIndex.y), sin(B * AdjustedIndex.y)) + fovealPoint;
            
                d = ((float2(logPolar2Screen.x, logPolar2Screen.y) / LaunchDimensions.xy) * 2.f - 1.f);
            }

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

    finalColor /= pow(float(params.sqrtSamplesPerPixel), 2);

    if (params.isFoveatedRenderingEnabled)
    {
        RTOutput[LaunchIndex.xy] = float4(finalColor, 1.0f);
    }
    else
    {
        fillOutputAtIndex(LaunchIndex.xy, float4(finalColor, 1.0f), maxCornerDist);
    }
    
}
