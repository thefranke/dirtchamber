/*
 * The Dirtchamber - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#define DVCT
#include "deferred_vct.hlsl"

Texture2D rt_kinect_color  : register(t0);
Texture2D rt_kinect_ldepth : register(t1);

float2 to_gbuffer(in float3 c)
{
    float4 pc = mul(vp, float4(c, 1.0));
        pc.xy /= pc.w;

    return to_tex(pc.xy);
}

bool is_real_surface(in float3 pos)
{
    float2 tc = to_gbuffer(pos);

    // TODO: abuse specular as mask for now, replace me later
    float v = rt_specular.Sample(StandardFilter, tc).r;

    return (v < 0.1);
}

float4 ps_ar_dvct(in PS_INPUT inp) : SV_Target
{
    float4 kinect_background = pow(rt_kinect_color.Sample(StandardFilter, inp.tex_coord).rgba, 2.2);
    float4 diffuse_albedo    = rt_colors.Sample(StandardFilter, inp.tex_coord);
    float4 specular_albedo   = rt_specular.Sample(StandardFilter, inp.tex_coord);
    float4 normal            = rt_normals.Sample(StandardFilter, inp.tex_coord);
    float  depth             = rt_lineardepth.Sample(StandardFilter, inp.tex_coord).r;
    uint   shading_mode      = round(normal.a * 10);

    // ignore parts with no normals
    if (dot(normal.xyz, normal.xyz) == 0)
        return kinect_background;

    float3 Na = normal.xyz * 2.0 - 1.0;
    float3 N = normalize(Na);

    // view
    float3 V = -normalize(inp.view_ray.xyz);

    // if diffuse_albedo has alpha = 0, the color is actually emissive
    if (shading_mode == 2)
        return diffuse_albedo*100;

    // reconstruct position in world space
    float3 P = camera_pos + (-V * depth);

    bool is_real = is_real_surface(P);

//#define DEBUG_NORMALS
#ifdef DEBUG_NORMALS
    float3 vP = mul(world_to_svo, float4(camera_pos, 1.0)).xyz;
    float3 vV = mul(world_to_svo, float4(-V, 0.0)).xyz;

    float4 x = trace_cone(vP, normalize(vV), 0.0001, 50, true, 1);

    if (x.w > 0.f)
        return x;
    //return x / 2.5 / 2.0 + 0.5;
    else
        return kinect_background;
#endif

//#define DEBUG_VOLUME
#ifdef DEBUG_VOLUME
    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;

    int4 lpos = int4(vP * SVO_SIZE, 0);

    return normalize(normal_volume.Load(lpos)).rgba / 2.0 + 0.5;
#endif

    // light
    float3 Ll = main_light.position.xyz - P;
    float3 L = normalize(Ll);

    // lambert
    float NoL = saturate(dot(N, L));

    // have no gloss maps, roughness is simply inverse spec color
    float roughness = 1.0 - specular_albedo.r;

    // calculate power of light and scale it to get the scene adequately lit
    float power = 100 * length(scene_dim_max - scene_dim_min);

    // calculate irradiance
    float3 Li = M_PI * (power * get_spotlight(L, N) * main_light.color.rgb) / dot(Ll, Ll);

    // shadow attenuation factor
    float attenuation = shadow_cone(P, L, N, 0.0005);

    // brdf
    float3 f = brdf(L, V, N, diffuse_albedo.rgb, specular_albedo.rgb, 1);

    // emissive
    float3 T0 = 0;

    // direct
    float3 T1 = f * Li * attenuation * NoL;

    if (is_real)
    {
        // NOTE: this might look nice, but erradicates all real indirect bounces from the image as well
        // T1 = kinect_background * attenuation
        // Correct way: use regular background, compute antiradiance from shadowed light source only and add this to T1
        T1 = kinect_background.rgb;
        f = brdf(L, V, N, diffuse_albedo.rgb, 0, 1);
        T1 -= kinect_background.rgb * f * (1 - attenuation);
    }

    // indirect bounce number one
    float3 T2 = float3(0, 0, 0);

    // first bounce
    float4 diffuse_indirect = diffuse_from_vct(inp.tex_coord.xy, P, N, V, is_real);
    T2 += diffuse_indirect.rgb * gi_scale * diffuse_albedo.rgb / M_PI;

    if (is_real)
    {
        float4 specular_indirect = specular_from_vct(P, N, V, glossiness, is_real);
        T2 += specular_indirect.rgb * gi_scale;
        T1 *= pow(1 - specular_indirect.a, 10);
    }

    if (debug_gi)
        return float4(abs(T2), 1.0f);

    return float4(max(T0 + T1 + T2, 0.0f), !is_real);
}
