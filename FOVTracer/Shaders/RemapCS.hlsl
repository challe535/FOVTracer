#include "KernelFov.hlsl"

#define PI 3.141592653589793
#define BLOCKSIZE 32

cbuffer ParamsCB : register(b0)
{
    float2 fovealCenter;
    bool isFoveatedRenderingEnabled;
    float kernelAlpha;
    float viewportRatio;
    float2 resolution;
    bool shouldBlur;
}

RWTexture2D<float4> logPolarBuffer : register(u0);
RWTexture2D<float4> remappedBuffer : register(u1);

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 LaunchIndex = float2(DTid.xy);
	
    float2 fovealPoint = fovealCenter / viewportRatio;

    float2 l1 = resolution - fovealPoint;
    float2 l2 = float2(l1.x, 0 - fovealPoint.y);
    float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
    float2 l4 = float2(0 - fovealPoint.x, l1.y);

    float maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
    float L = log(maxCornerDist);

    float2 relativePoint = LaunchIndex - fovealPoint;

    float u = kernelFuncInv(log(length(relativePoint)) / L, kernelAlpha) * resolution.x;
    float v = (atan2(relativePoint.y, relativePoint.x) + (relativePoint.y < 0 ? 1 : 0) * 2 * PI) * resolution.y / (2 * PI);
    
    float lu = u - 1;
    float ru = u + 1;
    float lv = (v - 1) % resolution.y;
    float rv = (v + 1) % resolution.y;

    //Use later for linear filtering sort of thing
    //uint iu = floor(u);
    //uint iv = floor(v);
    //float fu = frac(u);
    //float fv = frac(v);

    float3 finalColor = float3(0, 0, 0);

    if (isFoveatedRenderingEnabled)
    {

        finalColor = logPolarBuffer[float2(u, v)];

        if (shouldBlur)
        {
            finalColor += logPolarBuffer[float2(lu, v)];
            finalColor += logPolarBuffer[float2(ru, v)];
            finalColor += logPolarBuffer[float2(u, lv)];
            finalColor += logPolarBuffer[float2(u, rv)];

            finalColor += logPolarBuffer[float2(ru, rv)];
            finalColor += logPolarBuffer[float2(lu, rv)];
            finalColor += logPolarBuffer[float2(ru, lv)];
            finalColor += logPolarBuffer[float2(lu, lv)];

            finalColor /= 9;
        }

        float normFovealDist = length(relativePoint) / maxCornerDist;
        float upper = kernelFunc(0.1, kernelAlpha);
        float lower = kernelFunc(0.099, kernelAlpha);
        if (normFovealDist < upper && normFovealDist > lower)
        {
            finalColor = float3(1, 0, 1) * 0.5 + finalColor * 0.5;
        }
       
    }
    else
    {
        finalColor = logPolarBuffer[DTid.xy];
        if (shouldBlur)
        {
            finalColor += logPolarBuffer[float2(DTid.x - 1, DTid.y)];
            finalColor += logPolarBuffer[float2(DTid.x + 1, DTid.y)];
            finalColor += logPolarBuffer[float2(DTid.x, DTid.y - 1)];
            finalColor += logPolarBuffer[float2(DTid.x, DTid.y + 1)];

            finalColor += logPolarBuffer[float2(DTid.x - 1, DTid.y - 1)];
            finalColor += logPolarBuffer[float2(DTid.x + 1, DTid.y - 1)];
            finalColor += logPolarBuffer[float2(DTid.x - 1, DTid.y + 1)];
            finalColor += logPolarBuffer[float2(DTid.x + 1, DTid.y + 1)];

            finalColor /= 9;
        }
    }

    remappedBuffer[DTid.xy] = float4(finalColor, 1.0f);
        
}