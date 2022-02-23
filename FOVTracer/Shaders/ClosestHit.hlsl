/* Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Common.hlsl"

struct PointLightInfo
{
    float3 position;
    float3 color;
    float luminocity;
};

bool IsShadowed(float3 lightDir, float3 origin, float maxDist)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = maxDist;

    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

    TraceRay(
		SceneBVH,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		shadowPayload
	);

    return shadowPayload.isHit;
}

float3 CalcMappedNormal(VertexAttributes vertex, int2 coord)
{
    float3 mapping = (normals.Load(int3(coord, 0)).rgb - 0.5) * 2;
    return mapping.x * vertex.tangent + mapping.y * vertex.binormal + mapping.z * vertex.normal;
}

//float3 CalcOpacityColor()
//{
//    RayDesc ray;
//    ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() * 1.05f;
//    ray.Direction = WorldRayDirection();
//    ray.TMin = 0.01;
//    ray.TMax = 100000;

//    HitInfo payload;
//    payload.ShadedColorAndHitT = float4(0, 0, 0, 0);

//    TraceRay(
//		SceneBVH,
//		RAY_FLAG_NONE,
//		0xFF,
//		1,
//		1,
//		1,
//		ray,
//		payload
//	);

//    return payload.ShadedColorAndHitT.rgb;
//}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

    PointLightInfo plInfo;

    plInfo.position = float3(0, 700 + 100 * sin(elapsedTimeSeconds), 0);
    plInfo.color = float3(0.9, 0.9, 1.0);
    plInfo.luminocity = 1000000.0;

    int2 coord = floor(frac(vertex.uv) * material.textureResolution.xy);
  
	float4 diffuse = float4(1, 1, 0, 1);
    if (material.hasDiffuseTexture)
    {
        diffuse = albedo.Load(int3(coord, 0));
    }

    if (material.hasNormalMap)
    {
        vertex.normal = CalcMappedNormal(vertex, coord);
    }

    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.001f;
    float3 lightV = plInfo.position - worldOrigin;
    float3 lightDir = normalize(lightV);
    float distToLight = length(lightV);

    float ambient = 0.3;

    float factor = IsShadowed(lightDir, worldOrigin, distToLight) ? ambient : 1.0;

    //if (diffuse.a < 0.999)
    //{
    //    diffuse.rgb = diffuse.rgb * diffuse.a + CalcOpacityColor() * (1.0 - diffuse.a);
    //}

    float3 color = factor * diffuse.rgb * max(dot(lightDir, vertex.normal), ambient) * plInfo.color * plInfo.luminocity / pow(distToLight, 2);

    payload.ShadedColorAndHitT = float4(color, RayTCurrent());
	//payload.ShadedColorAndHitT = float4(vertex.uv, 0, RayTCurrent());
    //payload.ShadedColorAndHitT = float4(newnormal, RayTCurrent());
	// payload.ShadedColorAndHitT = float4(RayTCurrent(), RayTCurrent(), RayTCurrent(),RayTCurrent())/1000;
}