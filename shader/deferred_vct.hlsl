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

float2 to_gbuffer(in float3 c)
{
    float4 pc = mul(vp, float4(c, 1.0));
    pc.xy /= pc.w;

    return to_tex(pc.xy);
}

#ifdef DVCT
float4 specular_from_vct(in float3 P, in float3 N, in float3 V, float cone_angle, bool is_real)
#else
float4 specular_from_vct(in float3 P, in float3 N, in float3 V, float cone_angle)
#endif
{
    float3 R = reflect(-V, N);

    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;
    float3 vR = mul(world_to_svo, float4(R, 0.0)).xyz;
    float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

    // bias a bit to avoid self intersection
    vP += 4 * normalize(vN) / SVO_SIZE;

    float NdotL = saturate(dot(normalize(N), normalize(R)));

    // get more fingerained specular refs here
    float3 vvR = normalize(vR) / 0.5;

#ifdef DVCT
    float4 spec_bounce = trace_cone(vP, vvR, cone_angle, 11, 1, is_real);
#else
    float4 spec_bounce = trace_cone(vP, vvR, cone_angle, 11, 1);
#endif

    // TODO: add actual material properties here
    float3 f = brdf(R, V, N, 1, 1, 0);

    return float4(spec_bounce.rgb * NdotL * f.rgb, spec_bounce.w);
}

#ifdef DVCT
float4 diffuse_from_vct(in float3 P, in float3 N, in float3 V, bool is_real)
#else
float4 diffuse_from_vct(in float3 P, in float3 N, in float3 V)
#endif
{
    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;

    // TODO: generate better cones! This hack will likely fail at some point
    float3 diffdir = normalize(N.zxy);
    float3 crossdir = cross(N.xyz, diffdir);
    float3 crossdir2 = cross(N.xyz, crossdir);

    float3 directions[9] =
    {
        N,
        normalize(crossdir + N),
        normalize(-crossdir + N),
        normalize(crossdir2 + N),
        normalize(-crossdir2 + N),
        normalize(crossdir + crossdir2 + N),
        normalize(crossdir - crossdir2 + N),
        normalize(-crossdir + crossdir2 + N),
        normalize(-crossdir - crossdir2 + N),
    };

    float diff_angle = 0.6f;

    float4 diffuse = float4(0, 0, 0, 0);

#define num_d 9

    for (uint d = 0; d < num_d; ++d)
    {
        float3 D = directions[d];
        float3 vD = mul(world_to_svo, float4(D, 0.0)).xyz;
        float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

        vP += normalize(vN) / SVO_SIZE;

        float NdotL = saturate(dot(normalize(N), normalize(D)));

        // TODO: add actual material properties here
        float4 f = float4(brdf(normalize(D), V, N, 1, 0, 1), 1);

#ifdef DVCT
        diffuse += trace_cone(vP, normalize(vD), diff_angle, 5, 20, is_real) * NdotL * f;
#else
        diffuse += trace_cone(vP, normalize(vD), diff_angle, 2, 5) * NdotL * f;
#endif
    }

    diffuse *= 4.0 * M_PI / num_d;

    return diffuse/M_PI;
}

#ifndef DVCT
float4 gi_from_vct(in float3 P, in float3 N, in float3 V, in float3 diffuse, in float3 specular, in float a)
{
    a += glossiness / 10.f;

    return
        diffuse_from_vct(P, N, V) +
        specular_from_vct(P, N, V, a); 
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
    float3 T2 = gi_from_vct(P, N, V, gb.diffuse_albedo.rgb, gb.specular_albedo.rgb, roughness).rgb;

    T2 *= gi_scale;

    if (debug_gi)
        return float4(T2, 1.0);

    T2 *= gb.diffuse_albedo.rgb / M_PI;

    return float4(max(T0 + T1 + T2, 0.0f), 1.0f);
}
#endif