#include "Common.hlsl"

[shader("anyhit")]
void ShadowAnyHit(inout ShadowHitInfo hit, Attributes attrib)
{
    //if (material.hasTransparency)
    //{
    //    uint triangleIndex = PrimitiveIndex();
    //    float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
    //    VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

    //    int2 coord = floor(frac(vertex.uv) * material.textureResolution.xy);

    //    float alpha = opacity.Load(int3(coord, 0)).x;
        
    //    if (alpha < 1.0)
    //        return;
    //}
    
    //AcceptHitAndEndSearch();
}