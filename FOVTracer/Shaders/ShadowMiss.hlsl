#include "Common.hlsl"

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit)
{
	hit.isHit = false;
}