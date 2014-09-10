/* 
 * lpv_rendervol.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"

struct VS_MESH_INPUT
{
    float3 pos              : POSITION;
    float3 norm             : NORMAL;
	float2 texcoord 	    : TEXCOORD0;
	float3 tangent		    : TANGENT;
};

struct VS_MESH_OUTPUT
{
    float4 pos			    : SV_POSITION;
	float4 norm			    : NORMAL;
	float3 tangent		    : TANGENT;
    float2 texcoord         : TEXCOORD0;
    float3 ldepth           : LINEARDEPTH;
};

struct PS_MESH_OUTPUT
{
	float4 color		    : SV_Target0;
	float4 normal		    : SV_Target1;
	float2 ldepth		    : SV_Target2;
	float4 specular		    : SV_Target3;
};

Texture2DArray lpv_r        : register(t7);
Texture2DArray lpv_g        : register(t8);
Texture2DArray lpv_b        : register(t9);

cbuffer onetime_vs          : register(b2)
{
    float4 scene_dim_max    : packoffset(c0);
    float4 scene_dim_min    : packoffset(c1);
}

cbuffer debug               : register(b7)
{
    float3 lpv_pos          : packoffset(c0.x);
    float pad0              : packoffset(c0.w);
};

cbuffer camera_vs           : register(b0)
{
    float4x4 vp             : packoffset(c0);
    float4x4 vp_inv         : packoffset(c4);
    float3 camera_pos       : packoffset(c8);
    float pad1              : packoffset(c8.w);
}

cbuffer meshdata_vs         : register(b1)
{
    float4x4 world          : packoffset(c0);
}

VS_MESH_OUTPUT vs_rendervol(in VS_MESH_INPUT input)
{
	VS_MESH_OUTPUT output;

    float3 smax = scene_dim_max.xyz;
    float3 smin = scene_dim_min.xyz;
    
    // TODO: FIX THIS, LPV HAS ITS OWN SIZE!
    float3 diag = smax - smin;
    
    float3 dir = diag/float(LPV_SIZE);
    
    float s = length(dir)*0.4;

    float3 lpv_pos1 = lpv_pos + float3(1,1,1);

    float3 npos = smin + input.pos*s + dir*lpv_pos1.xyz;

    output.pos = mul(vp, mul(world, float4(npos, 1)));
	output.norm = float4(input.norm, 0);
	output.tangent = input.tangent;
    output.texcoord = input.texcoord;
	output.ldepth = 1.0;

    return output;
}

float4 load(in Texture2DArray lpv, in float3 lpv_pos)
{
    return lpv.Load(int4(lpv_pos.x, lpv_pos.y, lpv_pos.z, 0));
}

PS_MESH_OUTPUT ps_rendervol(in VS_MESH_OUTPUT input)
{ 
    PS_MESH_OUTPUT output;

    // query lpv for color
    float4 r = load(lpv_r, lpv_pos);
    float4 g = load(lpv_g, lpv_pos);
    float4 b = load(lpv_b, lpv_pos);
	
	float4 normal_lobe = sh_clamped_cos_coeff(float3(0.f, 0.f, -1.f));
	
    float3 nrgb;
    nrgb.r = dot(r, normal_lobe);
    nrgb.g = dot(g, normal_lobe);
    nrgb.b = dot(b, normal_lobe);

    if (nrgb.r < 0 || nrgb.g < 0 || nrgb.b < 0)
        nrgb = float3(1.0, 0.0, 0.0);
    
    output.color.rgb = nrgb;
    output.color.a = 0.0f;

    if (length(output.color.rgb) < 0.05f)
        discard;

    output.ldepth = 1.0;
    output.specular = float4(0,0,0,0);
    output.normal = float4(0,0,0,0);

    return output;
}
