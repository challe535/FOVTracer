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
    bool isMotionView;
    bool isDepthView;
}

RWTexture2D<float4> InColorBuffer : register(u0);
RWTexture2D<float4> OutColorBuffer : register(u1);
RWTexture2D<float4> InMotionBuffer : register(u2);
RWTexture2D<float4> OutMotionBuffer : register(u3);
RWTexture2D<float4> WorldPosBuffer : register(u4);


float4 sampleTexture(RWTexture2D<float4> buffer, float2 index)
{
    
    float4 result = buffer[index];

    if (shouldBlur)
    {
        result += buffer[float2(index.x - 1, index.y)];
        result += buffer[float2(index.x + 1, index.y)];
        result += buffer[float2(index.x, index.y - 1)];
        result += buffer[float2(index.x, index.y + 1)];

        result += buffer[float2(index.x - 1, index.y - 1)];
        result += buffer[float2(index.x + 1, index.y - 1)];
        result += buffer[float2(index.x - 1, index.y + 1)];
        result += buffer[float2(index.x + 1, index.y + 1)];

        result /= 9;
    }

    return result;
}

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

    //Use later for linear filtering sort of thing
    //uint iu = floor(u);
    //uint iv = floor(v);
    //float fu = frac(u);
    //float fv = frac(v);

    float3 finalColor = float3(0, 0, 0);

    if (isDepthView)
    {
        finalColor = sampleTexture(WorldPosBuffer, DTid.xy).aaa;

    }
    else if (isFoveatedRenderingEnabled)
    {

        finalColor = sampleTexture(InColorBuffer, float2(u, v));

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
        finalColor = sampleTexture(InColorBuffer, DTid.xy).rgb;
    }

    //Motion buffer overlay
    if (isMotionView)
    {
        finalColor.rgb *= 0.5f;
        finalColor.rg += float4(sampleTexture(InMotionBuffer, DTid.xy).xy, 0, 0).rg * 0.5f;
    }


    OutColorBuffer[DTid.xy] = float4(finalColor, 1.0f);
        
}