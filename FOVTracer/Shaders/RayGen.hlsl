#include "Common.hlsl"

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    float2 LaunchDimensions = float2(DispatchRaysDimensions().xy);
	
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
            float2 d = ((float2(LaunchIndex.x + offsetX, LaunchIndex.y + offsetY) / LaunchDimensions.xy) * 2.f - 1.f);
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

    RTOutput[LaunchIndex.xy] = float4(finalColor, 1.f);
    }
