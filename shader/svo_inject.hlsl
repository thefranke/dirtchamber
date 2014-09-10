/*
 * svo_inject.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"

SamplerState StandardFilter : register(s0);

struct VS_INJECT_OUTPUT
{
    float2 tc                               : POSITION;

    // this is only here to prevent D3D throwing warnings
    float4 pos						        : SV_POSITION;
};

cbuffer camera_vs                           : register(b0)
{
    float4x4 vp                             : packoffset(c0);
    float4x4 vp_inv                         : packoffset(c4);
    float3 camera_pos                       : packoffset(c8);
    float pad                               : packoffset(c8.w);
}

cbuffer light_ps                            : register(b2)
{
    directional_light main_light            : packoffset(c0);
    float4x4 light_mvp				        : packoffset(c3);
    float4x4 light_vp_inv			        : packoffset(c7);
    float4x4 light_vp_tex			        : packoffset(c11);
}

cbuffer parameters                          : register(b7)
{
    float4x4 world_to_svo                   : packoffset(c0);
    float4 bb_min                           : packoffset(c4);
    float4 bb_max                           : packoffset(c5);
    float4 pad2[2]                          : packoffset(c6);
};

Texture2D diffuse_tex                       : register(t0);

Texture2D rsm_rho_depth	                    : register(t6);
Texture2D rsm_rho_colors	                : register(t7);
Texture2D rsm_rho_normals	                : register(t8);

Texture3D normal_volume		                : register(t10);

Texture2D rsm_mu_depth		                : register(t12);
Texture2D rsm_mu_colors		                : register(t13);
Texture2D rsm_mu_normals		            : register(t14);

RWTexture3D<float4> v_rho	                : register(u1);
RWTexture3D<float4> v_delta	                : register(u2);

// TODO: needs to be set automatically
#define NVPLS 1024

VS_INJECT_OUTPUT vs_svo_inject(in uint id : SV_VertexID)
{
    VS_INJECT_OUTPUT output;

    uint nvpls = NVPLS;

    uint x = id % nvpls;
    uint y = id / nvpls;

    // texture coords of vpl
    output.tc = float2((float)x / (float)nvpls, (float)y / (float)nvpls);

    // this is only here to prevent D3D throwing warnings
    output.pos = float4(0, 0, 0, 1);

    return output;
}

float3 reconstruct_pos(in float2 tc, in float depth)
{
    float3 vr = mul(light_vp_inv, float4((tc.xy - 0.5) * float2(2.0, -2.0), 1.0, 1.0)).xyz;
    return main_light.position.xyz + normalize(vr) * depth;
}

float4 calc_bounce(in float3 color, in float3 P, in float3 N)
{
    float3 uL = (main_light.position.xyz - P);

    float3 L = normalize(uL);
    float NdotL = saturate(dot(N, L));

    // use normal from transformed input instead of volume: cheaper access, more accurate
    float4 bounce = float4(color / M_PI * main_light.color.rgb * NdotL, 0);

    return bounce;
}

uint3 calc_voxel(in float3 P)
{
    return floor(mul(world_to_svo, float4(P, 1)).xyz * SVO_SIZE);
}

void splat_rho(in uint3 voxel, in float4 bounce)
{
    v_rho[voxel] = bounce;
}

void splat_delta(in uint3 virtual_voxel, in uint3 real_voxel, in float4 virtual_bounce, in float4 real_bounce)
{
    v_delta[virtual_voxel] = virtual_bounce; // don't need to add real bounce here, since it's zero anyway
    v_delta[real_voxel] = -real_bounce; // don't need to add virtual bounce here, since it's zero anyway
}

void ps_svo_inject(in VS_INJECT_OUTPUT input)
{
    int2 itc = int2(input.tc * NVPLS);

    float depth = rsm_rho_depth[itc].r;
    float3 color = rsm_rho_colors[itc].rgb;

    float3 P = reconstruct_pos(input.tc, depth);
    float3 N = rsm_rho_normals[itc].xyz * 2.0 - 1.0;
    uint3 voxel = calc_voxel(P);
    float4 bounce = calc_bounce(color, P, N);

    splat_rho(voxel, bounce);
}

void ps_svo_delta_inject(in VS_INJECT_OUTPUT input)
{
    int2 itc = int2(input.tc * NVPLS);

    float mu_depth = rsm_mu_depth[itc].r;
    float3 color_mu = rsm_mu_colors[itc].rgb;

    float3 P_mu = reconstruct_pos(input.tc, mu_depth);
    float3 N_mu = rsm_mu_normals[itc].xyz * 2.0 - 1.0;
    uint3 mu_voxel = calc_voxel(P_mu);
    float4 mu_bounce = calc_bounce(color_mu, P_mu, N_mu);

    float rho_depth = rsm_rho_depth[itc].r;
    float3 color_rho = rsm_rho_colors[itc].rgb;

    float3 P_rho = reconstruct_pos(input.tc, rho_depth);
    float3 N_rho = rsm_rho_normals[itc].xyz * 2.0 - 1.0;
    uint3 rho_voxel = calc_voxel(P_rho);
    float4 rho_bounce = calc_bounce(color_rho, P_rho, N_rho);

    bool is_virtual_surface = abs(rho_depth - mu_depth) > 0;

    if (is_virtual_surface)
        splat_delta(rho_voxel, mu_voxel, rho_bounce, mu_bounce);

    splat_rho(rho_voxel, rho_bounce);
}