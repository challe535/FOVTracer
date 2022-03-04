#include "Common.hlsl"

//Technique comes from Krüger, J., M. Niessner, and J. Stückler. "Multi-Layer Alpha Tracing." (2020).

Node Merge(Node n1, Node n2)
{
    Node n;
    n.depth = min(n1.depth, n2.depth);
    n.transmit = n1.transmit * n2.transmit;

    float w1 = n1.transmit / (n1.transmit + n2.transmit);
    float w2 = n2.transmit / (n1.transmit + n2.transmit);;
    n.color = (1 - w1) * n1.color + (1 - w2) * n2.color;

    return n;
}

void Swap(in Node n, out Node sn)
{
    sn.depth = n.depth;
    sn.transmit = n.transmit;
    sn.color = n.color;
}

[shader("anyhit")]
void AlphaAnyHit(inout HitInfo payload, Attributes attrib)
{
    //Safe guard, shouldn't be needed since only non-opaques call any hit shaders
    if (!material.hasTransparency)
        return;

    uint triangleIndex = PrimitiveIndex();
    float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
    VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

    int2 coord = floor(frac(vertex.uv) * material.textureResolution.xy);
    
    float4 diffuse = float4(1, 1, 0, 1);
    if (material.hasDiffuseTexture)
    {
        diffuse = albedo.Load(int3(coord, 0));
    }

    float alpha = opacity.Load(int3(coord, 0)).x;

    if (alpha == 0.0)
        IgnoreHit();

    Node n;
    n.depth = RayTCurrent();
    n.transmit = 1.0 - alpha;
    n.color = alpha * diffuse.rgb;

    // 1 - pass BACK insertion
    [ unroll ]
    for (int i = K - 1; i >= 0; --i)
        if (n.depth > payload.node[i].depth)
            Swap(n, payload.node[i]);

    //Causes overlap issues that are addressed in the paper but I'm not gonna bother...
    //if (i == -1)
    //    payload.node[0] = Merge(n, payload.node[0]);

    // this fragment was an occluder
    if (alpha == 1.0)
        return;

    // sufficient occlusion to cull nodes ?
    float transmit = 1.0;
    [ unroll ]
    for (i = 0; i < K; ++i)
        transmit *= payload.node[i].transmit;

    if (transmit <= 0.001 && payload.node[K - 1].depth <= RayTCurrent())
        return; // everything beyond is occluded
    IgnoreHit();

}