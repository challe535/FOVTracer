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
    bool isWorldPosView;
}

RWTexture2D<float4> InColorBuffer : register(u0);
RWTexture2D<float4> OutColorBuffer : register(u1);
RWTexture2D<float2> InMotionBuffer : register(u2);
RWTexture2D<float2> OutMotionBuffer : register(u3);
RWTexture2D<float4> WorldPosBuffer : register(u4);
RWTexture2D<float> DepthOutBuffer : register(u5);


float4 sampleTexture(RWTexture2D<float4> buffer, float2 index)
{
    
    float4 result = buffer[index];

    if (shouldBlur)
    {
        result += buffer[float2(index.x - 1, index.y) % resolution];
        result += buffer[float2(index.x + 1, index.y) % resolution];
        result += buffer[float2(index.x, index.y - 1) % resolution];
        result += buffer[float2(index.x, index.y + 1) % resolution];

        result += buffer[float2(index.x - 1, index.y - 1) % resolution];
        result += buffer[float2(index.x + 1, index.y - 1) % resolution];
        result += buffer[float2(index.x - 1, index.y + 1) % resolution];
        result += buffer[float2(index.x + 1, index.y + 1) % resolution];

        result /= 9;
    }

    return result;
}

float2 sampleTexture(RWTexture2D<float2> buffer, float2 index)
{
    
    float2 result = buffer[index];

    if (shouldBlur)
    {
        result += buffer[float2(index.x - 1, index.y) % resolution];
        result += buffer[float2(index.x + 1, index.y) % resolution];
        result += buffer[float2(index.x, index.y - 1) % resolution];
        result += buffer[float2(index.x, index.y + 1) % resolution];

        result += buffer[float2(index.x - 1, index.y - 1) % resolution];
        result += buffer[float2(index.x + 1, index.y - 1) % resolution];
        result += buffer[float2(index.x - 1, index.y + 1) % resolution];
        result += buffer[float2(index.x + 1, index.y + 1) % resolution];

        result /= 9;
    }

    return result;
}

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 LaunchIndex = float2(DTid.xy);
	
    float3 finalColor = float3(0, 0, 0);
    float depth = -1;
    float2 motion = float2(0, 0);

    float2 sampleIndex = LaunchIndex;
    float maxCornerDist;
    float2 relativePoint;
    if (isFoveatedRenderingEnabled)
    {
        float2 fovealPoint = fovealCenter * resolution;

        float2 l1 = resolution - fovealPoint;
        float2 l2 = float2(l1.x, 0 - fovealPoint.y);
        float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
        float2 l4 = float2(0 - fovealPoint.x, l1.y);

        maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
        float L = log(maxCornerDist);

        relativePoint = LaunchIndex - fovealPoint;

        float u = kernelFuncInv(log(length(relativePoint)) / L, kernelAlpha) * resolution.x;
        float v = (atan2(relativePoint.y, relativePoint.x) + (relativePoint.y < 0 ? 1 : 0) * 2 * PI) * resolution.y / (2 * PI);

        sampleIndex = float2(u, v);

    }

    finalColor = sampleTexture(InColorBuffer, sampleIndex).rgb;
    depth = sampleTexture(WorldPosBuffer, sampleIndex).a;
    motion = sampleTexture(InMotionBuffer, sampleIndex).xy;

    if (isWorldPosView)
    {
        finalColor = sampleTexture(WorldPosBuffer, sampleIndex).xyz;
    }
    else if (isDepthView)
    {
        finalColor = float3(depth, depth, depth);
    }
    else if (isMotionView)
    {
        finalColor.rg += motion;
        finalColor.rgb *= 0.5f;
    }

    //Ring around foveal area
    if (isFoveatedRenderingEnabled)
    {
        float normFovealDist = length(relativePoint) / maxCornerDist;
        float upper = kernelFunc(0.1, kernelAlpha);
        float lower = kernelFunc(0.099, kernelAlpha);
        if (normFovealDist < upper && normFovealDist > lower)
        {
            finalColor = float3(1, 0, 1) * 0.5 + finalColor * 0.5;
        }
    }

    OutMotionBuffer[DTid.xy] = motion;
    DepthOutBuffer[DTid.xy] = depth;
    OutColorBuffer[DTid.xy] = float4(finalColor, 1.0f);
}