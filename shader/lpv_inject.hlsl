/* 
 * lpv_inject.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"

#define VPLS 1024

SamplerState VPLFilter          : register(s0);

struct VS_LPV_INJECT
{
    float4 pos                  : POSITION;
    float3 normal               : NORMAL;
    float3 color                : COLOR;
};

struct GS_LPV_INJECT
{
    float4 pos                  : SV_Position; // 2D slice vertex coordinates in homogenous clip space
    float3 normal               : NORMAL;
    float3 color                : COLOR;
    uint rtindex                : SV_RenderTargetArrayIndex;  // used to choose the destination slice
};

struct PS_LPV_INJECT
{
    float4 coeff_r              : SV_Target0;
    float4 coeff_g              : SV_Target1;
    float4 coeff_b              : SV_Target2;
    float  count                : SV_Target3;
};

cbuffer light_vs                : register(b0)
{
    float4x4 light_vp           : packoffset(c0);
    float4x4 light_vp_inv       : packoffset(c4);
    float3 main_light           : packoffset(c8);
    float pad0                  : packoffset(c8.w);
}

cbuffer lpv_parameters          : register(b7)
{
    float4x4 world_to_lpv       : packoffset(c0);
    uint lpv_size               : packoffset(c4.x);
    float3 pad1                 : packoffset(c4.y);
}

cbuffer delta_inject            : register(b6)
{
    float dscale                : packoffset(c0.x);
    bool is_direct              : packoffset(c0.y);
    float2 pad2                 : packoffset(c0.z);
}

Texture2D rt_rsm_lineardepth    : register(t6);
Texture2D rt_rsm_colors         : register(t7);
Texture2D rt_rsm_normals        : register(t8);

VS_LPV_INJECT vs_lpv_inject(in uint id : SV_VertexID)
{
    VS_LPV_INJECT output;

    bool outside = false;

    uint nvpls = VPLS;

    uint x = id % nvpls;
    uint y = id / nvpls;
    
    // texture coords of vpl
    float2 tc = float2((float)x/(float)nvpls, (float)y/(float)nvpls);

    // get vpl
    directional_light vpl = gen_vpl(tc, light_vp_inv, float4(main_light, 1.0), rt_rsm_colors, rt_rsm_normals, rt_rsm_lineardepth, VPLFilter);

    float4 ppos = mul(world_to_lpv, vpl.position);
    float4 pnor = normalize(mul(world_to_lpv, vpl.normal));

    // shift by half a cell size into direction of normal
    ppos += pnor * (0.5/lpv_size);

    // create z index into array
    ppos.z *= lpv_size;

    // kill accidental injects with wrong normals
    float4 d = (float4(main_light, 1.0) - vpl.position);
    if (dot(vpl.normal, normalize(d)) < 0)
        ppos *= 50000;

    float3 w = main_light - vpl.position.xyz;

	output.pos    = float4(ppos.xyz, 1.0);
    output.color  = vpl.color.rgb/M_PI * saturate(dot(normalize(w), vpl.normal.xyz));
	output.normal = vpl.normal.xyz;

    return output;
}

VS_LPV_INJECT vs_delta_lpv_inject(in uint id : SV_VertexID)
{
    VS_LPV_INJECT output;

    bool outside = false;

    uint nvpls = VPLS;

    uint x = id % nvpls;
    uint y = id / nvpls;
    
    // texture coords of vpl
    float2 tc = float2((float)x/(float)nvpls, (float)y/(float)nvpls);

    // get vpl
    directional_light vpl = gen_vpl(tc, light_vp_inv, float4(main_light, 1.0), rt_rsm_colors, rt_rsm_normals, rt_rsm_lineardepth, VPLFilter);

    float4 ppos = mul(world_to_lpv, vpl.position);
    
    float4 w = float4(main_light, 1.0) - vpl.position;
    float4 nw = normalize(w);
    
    // shift by half a cell size into direction of normal
    ppos += vpl.normal * (0.5/lpv_size);
    ppos.y += 1.0/lpv_size;
    
    // create z index into array
    ppos.z *= lpv_size;
    
    // kill accidental injects with wrong normals
    float4 d = (float4(main_light, 1.0) - vpl.position);
    if (dot(vpl.normal, normalize(d)) < 0)
        ppos *= 50000;

    float scale = dscale / 10;

    output.pos    = float4(ppos.xyz, 1.0);
    output.color = vpl.color.rgb / 3.1415 * scale * saturate(dot(nw, vpl.normal));
    output.normal = vpl.normal.xyz;

    return output;
}

VS_LPV_INJECT vs_delta_lpv_direct_inject(in uint id : SV_VertexID)
{
    VS_LPV_INJECT output;
    
    bool outside = false;

    uint nvpls = VPLS;

    uint x = id % nvpls;
    uint y = id / nvpls;
    
    // texture coords of vpl
    float2 tc = float2((float)x/(float)nvpls, (float)y/(float)nvpls);

    // get vpl
    directional_light vpl = gen_vpl(tc, light_vp_inv, float4(main_light, 1.0), rt_rsm_colors, rt_rsm_normals, rt_rsm_lineardepth, VPLFilter);

    float4 ppos = mul(world_to_lpv, vpl.position);

    float4 w = float4(main_light, 1.0) - vpl.position;
    float4 nw = normalize(w);
    
    ppos += vpl.normal * (0.5/lpv_size);
    
    // create z index into array
    ppos.z *= lpv_size;
    
    // kill accidental injects with wrong normals
    float4 d = (float4(main_light, 1.0) - vpl.position);
    if (dot(vpl.normal, normalize(d)) < 0)
        ppos *= 50000;

    output.pos    = float4(ppos.xyz, 1.0);
    
    // for some reason, putting float3(1,1,1) instead of saturate(vpl.color.rgb * 10) adds 0.5ms extra!
    output.color = saturate(vpl.color.rgb * 0.01) * dscale;

    output.normal = -nw.xyz;

    return output;
}

[maxvertexcount (1)]
void gs_lpv_inject(in point VS_LPV_INJECT input[1], inout PointStream<GS_LPV_INJECT> stream)
{
    GS_LPV_INJECT output;

	// THINK: mayhaps create guard against invalid input points
	output.rtindex     = floor(input[0].pos.z);
	output.pos         = float4(input[0].pos.x * 2.0 - 1.0, (1.0 - input[0].pos.y) * 2.0 - 1.0, 0.0f, 1.0f);
	output.normal      = input[0].normal;
	output.color       = input[0].color;

    stream.Append(output);
}

PS_LPV_INJECT ps_lpv_inject(in GS_LPV_INJECT input)
{
    PS_LPV_INJECT output;

    float3 normal = normalize(input.normal);
    float4 coeffs = sh_clamped_cos_coeff(normal);

    output.coeff_r = input.color.r * coeffs;
    output.coeff_g = input.color.g * coeffs;
    output.coeff_b = input.color.b * coeffs;

    // increase number of injects in this cell
    output.count = 1;

    return output;
}

PS_LPV_INJECT ps_delta_lpv_inject(in GS_LPV_INJECT input)
{
    PS_LPV_INJECT output;

    float3 normal = normalize(input.normal);
    float4 coeffs = sh_clamped_cos_coeff(normal);

    output.coeff_r = input.color.r * coeffs;
    output.coeff_g = input.color.g * coeffs;
    output.coeff_b = input.color.b * coeffs;

    // increase number of injects in this cell
    output.count = 1;

    return output;
}
