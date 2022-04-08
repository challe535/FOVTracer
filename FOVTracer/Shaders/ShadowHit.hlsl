#include "Common.hlsl"

[shader("closesthit")]
void ShadowHit(inout ShadowHitInfo hit, Attributes bary)
{
    hit.isHit = true;
}
