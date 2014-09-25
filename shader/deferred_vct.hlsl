/*
 * deferred_vct.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "deferred.hlsl"
#include "vct_tools.hlsl"

cbuffer gi_parameters_ps    : register(b3)
{
    float gi_scale          : packoffset(c0.x);
    float glossiness        : packoffset(c0.y);
    uint num_vpls		    : packoffset(c0.z);
    bool debug_gi           : packoffset(c0.w);
}

#ifndef DVCT
float4 gi_from_vct(in float2 tc, in float3 P, in float3 N, in float3 V, in float3 diffuse, in float3 specular, in float a)
{
    a += glossiness / 10.f;

    return diffuse_from_vct(tc, P, N, V) * float4(diffuse, 1) + 
           specular_from_vct(P, N, V, a) * float4(specular, 1);
}

float4 ps_vct(in PS_INPUT inp) : SV_Target
{
    gbuffer gb = unpack_gbuffer(inp.tex_coord);

    // normal
    float3 N = normalize(gb.normal.xyz * 2.0 - 1.0);

    // view
    float3 V = -normalize(inp.view_ray.xyz);

// DEBUG VOLUME
#if 0
    float3 vP = mul(world_to_svo, float4(camera_pos, 1.0)).xyz;
    float3 vV = mul(world_to_svo, float4(-V, 0.0)).xyz;
    return trace_cone(vP, normalize(vV), 0.001, 50, 0.1);
#endif

    // if shading_mode == 2, the color is actually emissive
    if (gb.shading_mode == 2)
        return gb.diffuse_albedo;

    // ignore parts with no normals
    if (dot(gb.normal, gb.normal) == 0)
        return gb.diffuse_albedo * float4(main_light.color.rgb, 1.0);

    // reconstruct position in world space
    float3 P = camera_pos + (-V * gb.depth);

    // light
    float3 Ll = main_light.position.xyz - P;
    float3 L = normalize(Ll);

    // lambert
    float NoL = saturate(dot(N, L));

    // have no gloss maps, roughness is simply inverse spec color/smoothness
    float roughness = 1.0 - gb.specular_albedo.r;

    // calculate power of light and scale it to get the scene adequately lit
    float power = 100 * length(scene_dim_max - scene_dim_min);

    // calculate irradiance
    float3 Li = M_PI * (power * get_spotlight(L, N) * main_light.color.rgb) / pow(length(Ll),2);

    // shadow attenuation factor
#if 1
    float attenuation = shadow_attenuation(P, Ll, rt_rsm_lineardepth, 0.0, 0.0);
#else
    float attenuation = shadow_cone(P, L, N, 0.00002); 
#endif

    // brdf
    float3 f = brdf(L, V, N, gb.diffuse_albedo.rgb, gb.specular_albedo.rgb, roughness);

    // emissive
    float3 T0 = 0;

    // direct
    float3 T1 = f * Li * attenuation * NoL;

    // indirect
    float3 T2 = gi_from_vct(inp.tex_coord.xy, P, N, V, gb.diffuse_albedo.rgb, gb.specular_albedo.rgb, roughness).rgb;

    T2 *= gi_scale;

    if (debug_gi)
        return float4(T2, 1.0);

    return float4(max(T0 + T1 + T2, 0.0f), 1.0f);
}
#endif