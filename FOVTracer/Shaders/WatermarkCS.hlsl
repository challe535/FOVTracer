#define BLOCKSIZE 32

RWTexture2D<float4> ColorBuffer : register(u0);

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 PixelCoord = float2(DTid.xy);
    
    if (PixelCoord.x > 1200 && PixelCoord.y > 1010 &&
        PixelCoord.x < 1900 && PixelCoord.y < 1040)
        ColorBuffer[PixelCoord] = float4(0, 0, 0, 0);
}