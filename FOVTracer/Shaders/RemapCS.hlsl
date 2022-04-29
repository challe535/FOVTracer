#include "KernelFov.hlsl"

#define PI 3.141592653589793
#define BLOCKSIZE 32

cbuffer ParamsCB : register(b0)
{
    float2 fovealCenter;
    bool isFoveatedRenderingEnabled;
    float kernelAlpha;
    float2 resolution;
    float2 jitterOffset;
    float blurKInner;
    float blurKOuter;
    float blurA;

    bool isMotionView;
    bool isDepthView;
    bool isWorldPosView;
    
    bool disableTAA;
    bool usingDLSS;
    bool takingReferenceScreenshot;
    float foveationAreaThreshold;
}

RWTexture2D<float4> InColorBuffer   : register(u0);
RWTexture2D<float4> InColorBuffer1  : register(u1);
RWTexture2D<float4> InColorBuffer2  : register(u2);
RWTexture2D<float4> InColorBuffer3  : register(u3);
RWTexture2D<float4> InColorBuffer4  : register(u4);
RWTexture2D<float4> OutColorBuffer  : register(u5);
RWTexture2D<float2> InMotionBuffer  : register(u6);
RWTexture2D<float2> OutMotionBuffer : register(u7);
RWTexture2D<float4> WorldPosBuffer  : register(u8);
RWTexture2D<float> DepthOutBuffer   : register(u9);


float4 sampleTexture(RWTexture2D<float4> buffer, float2 index, int kernelSize, float normFovealDist)
{
    float4 result = 0;
    
    //kernelSize *= kernelSizeFade;
    //kernelSize = max(kernelSize, 1);
    
    if (isFoveatedRenderingEnabled && normFovealDist > foveationAreaThreshold)
    {
        int kernelCenter = kernelSize / 2;
        float sigma = 0.85 * kernelCenter;
        float kernelSum = 0;
        
        float radiusFade = min(max(normFovealDist - foveationAreaThreshold, 0) * 5, 1);
        float radius = sqrt(kernelCenter * kernelCenter * 2) * radiusFade;
        int steps = kernelSize;
        float stepSize = radius / steps;
        int arcs = kernelSize * 2;
        
        for (int i = 0; i < arcs; i++)
        { 
            for (int j = 1; j <= steps; j++)
            {
                float angle = i * 2 * PI / arcs;
                float2 sampleOffset = float2(cos(angle), sin(angle)) * j * stepSize;
                
                float t = -(pow(sampleOffset.x, 2) + pow(sampleOffset.y, 2)) / (2 * pow(sigma, 2));
                float gauss = clamp(exp(t) / (2 * PI * pow(sigma, 2)), 0, 1);

                result += buffer[(index + sampleOffset) % resolution] * gauss;
                kernelSum += gauss;
            }
        }
       
        result /= kernelSum;
    }
    else
    {
        result = buffer[index];
    }

    return result;
}

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 LaunchIndex = float2(DTid.xy) + 0.5;
	
    float3 finalColor = float3(0, 0, 0);
    float depth = -1;
    float2 motion = float2(0, 0);

    float2 sampleIndex = LaunchIndex;
    float maxCornerDist;
    float2 relativePoint;

    float kernelSize = 0;

    float normFovealDist = -1;
    if (isFoveatedRenderingEnabled)
    {
        //LaunchIndex += jitterOffset * (takingReferenceScreenshot ? 0 : 1);
        
        float2 fovealPoint = fovealCenter * resolution;

        float2 l1 = resolution - fovealPoint;
        float2 l2 = float2(l1.x, 0 - fovealPoint.y);
        float2 l3 = float2(0 - fovealPoint.x, 0 - fovealPoint.y);
        float2 l4 = float2(0 - fovealPoint.x, l1.y);

        maxCornerDist = max(max(length(l1), length(l2)), max(length(l3), length(l4)));
        float L = log(maxCornerDist);

        relativePoint = LaunchIndex - fovealPoint;

        float uNorm = kernelFuncInv(log(length(relativePoint)) / L, kernelAlpha);
        float u = uNorm * resolution.x;
        float v = (atan2(relativePoint.y, relativePoint.x) + (relativePoint.y < 0 ? 1 : 0) * 2 * PI) * resolution.y / (2 * PI);

        normFovealDist = length(relativePoint) / maxCornerDist;

        if (normFovealDist > foveationAreaThreshold)
            sampleIndex = float2(u, v);

        float c = 1.8;
        float t = ceil(blurA - uNorm);
        float kInner = blurKInner;
        float kOuter = blurKOuter;
        float inner = kInner * pow(uNorm - blurA, 2) + c;
        float outer = kOuter * pow(uNorm - blurA, 2) + c;
        
        float n = t * inner + (1 - t) * outer;

        kernelSize = ceil(1 + 2 * n);
        
        if (normFovealDist > foveationAreaThreshold)
            finalColor = sampleTexture(InColorBuffer, sampleIndex, kernelSize, normFovealDist).rgb;
        else
            finalColor = sampleTexture(InColorBuffer2, sampleIndex, kernelSize, normFovealDist).rgb;
    }
    else
    {
        finalColor = sampleTexture(InColorBuffer, sampleIndex, kernelSize, normFovealDist).rgb;
    }


    
    depth = WorldPosBuffer[sampleIndex].a;
    motion = InMotionBuffer[sampleIndex].xy;

    if (isWorldPosView)
    {
        finalColor = sampleTexture(WorldPosBuffer, sampleIndex, kernelSize, normFovealDist).xyz;
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

    ////Ring around foveal area
    //if (isFoveatedRenderingEnabled)
    //{
    //    float normFovealDist = length(relativePoint) / maxCornerDist;
    //    float upper = kernelFunc(0.1, kernelAlpha);
    //    float lower = kernelFunc(0.099, kernelAlpha);
    //    if (normFovealDist < upper && normFovealDist > lower)
    //    {
    //        finalColor = float3(1, 0, 1) * 0.5 + finalColor * 0.5;
    //    }
    //}

    OutMotionBuffer[DTid.xy] = motion;
    DepthOutBuffer[DTid.xy] = depth;
    
    float2 historyIndex = LaunchIndex + motion;
    
    bool shouldAdjustTAAForFOV = usingDLSS && isFoveatedRenderingEnabled;
    float alpha = shouldAdjustTAAForFOV ? 0.15 : 0.4;
    
    float TAAThreshold = foveationAreaThreshold > 0 ? foveationAreaThreshold : 0.2;
    float TAAOffsetFOV = pow(max(((1 - alpha) / TAAThreshold) * (TAAThreshold - normFovealDist), 0.0), 2) * (shouldAdjustTAAForFOV ? 1 : 0);
    
    if (historyIndex.x >= resolution.x || historyIndex.x < 0 || historyIndex.y >= resolution.y || historyIndex.y < 0 || disableTAA)
        OutColorBuffer[DTid.xy] = float4(finalColor, 1.0f);
    else
        OutColorBuffer[DTid.xy] = float4(finalColor * (alpha + TAAOffsetFOV) + InColorBuffer1[historyIndex].rgb * (1 - alpha - TAAOffsetFOV), 1.0f);
}