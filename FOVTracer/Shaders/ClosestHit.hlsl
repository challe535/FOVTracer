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

// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

	//const float3 lightPos = float3(0, 10, 0);

	//float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection() + vertex.normal * 0.001f;
	//float3 lightDir = normalize(lightPos - worldOrigin);

	//RayDesc ray; 
	//ray.Origin = worldOrigin; 
	//ray.Direction = lightDir; 
	//ray.TMin = 0.01;
	//ray.TMax = 100000; 
	//bool hit = true;

	//ShadowHitInfo shadowPayload;
	//shadowPayload.isHit = false;

	//TraceRay( 
	//	SceneBVH, 
	//	RAY_FLAG_NONE, 
	//	0xFF, 
	//	0, 
	//	0, 
	//	0, 
	//	ray, 
	//	shadowPayload
	//); 
	//float factor = shadowPayload.isHit ? 0.3 : 1.0; 

	//int2 coord = floor(vertex.uv * textureResolution.xy);
	//float3 color = factor * albedo.Load(int3(coord, 0)).rgb * max(dot(lightDir, vertex.normal), 0.0);

	//payload.ShadedColorAndHitT = float4(color, RayTCurrent());

	payload.ShadedColorAndHitT = float4(1, 0, 0, RayTCurrent());
}