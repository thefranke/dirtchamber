/*
 * The Dirtchamber - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "deferred.hlsl"
#include "lpv_tools.hlsl"
#include "importance.hlsl"

cbuffer gi_parameters_ps        : register(b3)
{
    float gi_scale              : packoffset(c0.x);
    float lpv_flux_amplifier    : packoffset(c0.y);
    uint num_vpls               : packoffset(c0.z);
    bool debug_gi               : packoffset(c0.w);
}

Texture2D env_map               : register(t13);

float4 ps_lpv(PS_INPUT inp) : SV_Target
{
    gbuffer gb = unpack_gbuffer(inp.tex_coord);

    float3 Na = gb.normal.xyz * 2.0 - 1.0;
    float3 N = normalize(Na);

    // view
    float3 V = -normalize(inp.view_ray.xyz);

    // if diffuse_albedo has alpha = 0, the color is actually emissive
    if (gb.diffuse_albedo.a == 0.0)
        return gb.diffuse_albedo;

    // ignore parts with no normals
    if (dot(gb.normal, gb.normal) == 0)
        return gb.diffuse_albedo * float4(main_light.color.rgb, 1.0);

    if (gb.depth >= z_far)
        return float4(0.0, 0.0, 0.0, 0.0);

    // reconstruct position in world space
    float3 pos = camera_pos + (-V * gb.depth);

    // light
    float3 Ll = main_light.position.xyz - pos;
    float3 L = normalize(Ll);

    // lambert
    float NoL = dot(N, L);

    // reflected L
    float3 R = normalize(reflect(-L, N));

    // half vector between L and V
    float3 H = normalize(L+V);

    // have no gloss maps, roughness is simply inverse spec color
#if 1
    float roughness = 1.0 - gb.specular_albedo.r;
#else
    float roughness = gb.specular_albedo.a;
#endif

    // calculate power of light and scale it to get the scene adequately lit
    float power = 100 * length(scene_dim_max - scene_dim_min);

    // calculate irradiance
    float3 Li = M_PI * (power * get_spotlight(L, N) * main_light.color.rgb) / dot(Ll, Ll);

    // shadow attenuation factor
    float attenuation = shadow_attenuation(pos, Ll, rt_rsm_lineardepth, 0.0, 0.0);

    // brdf
#if 1
    float alpha = roughness*roughness;

    float NoV = dot(N, V);
    float NoH = dot(N, H);
    float LoH = dot(L, H);

    // refractive index
    float n = 1.5;
    float f0 = pow((1 - n)/(1 + n), 2);

    float3 Rs;

    if (gb.shading_mode == 1)
    {
        // the fresnel term
        float F = F_schlick(f0, LoH);

        // the geometry term
        float G = G_UE4(alpha, NoV);

        // the NDF term
        float D = D_ggx(alpha, NoH);

        // specular term
        Rs = gb.specular_albedo.rgb/M_PI *
                    (F * G * D)/
                    (4 * NoL * NoV);
    }
    else
    {
        roughness = gb.specular_albedo.a * (num_vpls - 1.0) / 512.0f;

        gb.specular_albedo.rgb = float3(F0_GOLD);
#if 1
        Rs = specular_ibl_is(gb.specular_albedo.rgb, roughness, N, V, env_map, StandardFilter) * main_light.color.rgb;
#else
        Rs = ibl_specular_blinn_phong_mip(gb.diffuse_albedo.rgb, gb.specular_albedo.rgb, roughness, N, V, env_map, StandardFilter) * main_light.color.rgb;
#endif
    }

    // diffuse fresnel, can be cheaper as 1-f0
    float Fd = F_schlick(f0, NoL);

    float3 Rd = gb.diffuse_albedo.rgb/M_PI * (1.0f - Fd);

    float3 f = (Rd + Rs);
#else
    float3 f = brdf(L, V, N, gb.diffuse_albedo.rgb, gb.specular_albedo.rgb, roughness);
#endif

    // emissive
    float3 T0 = 0;

    // direct
    float3 T1 = f * Li * attenuation * NoL;

    // first bounce
    float3 indirect = gi_from_lpv(pos, N).rgb * gi_scale;

    if (debug_gi)
        return float4(indirect, 1);

    float3 T2 = indirect * gb.diffuse_albedo.rgb / M_PI;

    return float4(max(T0 + T1 + T2, 0.0f), 1.0f);
}
