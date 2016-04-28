/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "deferred_lpv.hlsl"
#include "tonemapping.hlsl"

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

Texture2DArray lpv_rho_r : register(t11);
Texture2DArray lpv_rho_g : register(t12);
Texture2DArray lpv_rho_b : register(t13);

float get_spotlight(in float3 P, in float3 L, in float3 N)
{
    float3 spot_dir = normalize(main_light.normal.xyz);

        float falloff = 0.7;

    float alpha = dot(-L, spot_dir);

    float attenuation_dist = 1.0;

    if (alpha < falloff)
        attenuation_dist = 1.0 - 10 * abs(alpha - falloff);

    return 1;
    return sdot(L, N);
    return sdot(L, N) * saturate(attenuation_dist);
}

// returns rgba. a = 1 if pixel was processed normally, a = 0 if pixel wasn't processed at all or if it was phantom geometry
float4 ps_ar_dlpv(in PS_INPUT inp) : SV_Target
{
    float4 kinect_background = pow(rt_kinect_color.Sample(StandardFilter, inp.tex_coord).rgba, 2.2);
    float4 diffuse_albedo = rt_colors.Sample(StandardFilter,       inp.tex_coord);
    float3 normal  = rt_normals.Sample(StandardFilter,             inp.tex_coord).xyz;
    float  depth   = rt_lineardepth.Sample(StandardFilter,         inp.tex_coord).r;
    float4 specular_albedo    = rt_specular.Sample(StandardFilter, inp.tex_coord);

    // ignore parts with no normals
    if (dot(normal, normal) == 0)
        return kinect_background;

    float3 Na = normal * 2.0 - 1.0;
    float3 N = normalize(Na);

    // view
    float3 V = -normalize(inp.view_ray.xyz);

    // reconstruct position in world space
    float3 P = camera_pos + (-V * depth);

    bool is_real = is_real_surface(P);

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
    float3 Li = M_PI * (power * get_spotlight(P, L, N) * main_light.color.rgb) / dot(Ll, Ll);

    // shadow attenuation factor
    float attenuation = 1;

    // brdf
    float3 f = brdf(L, V, N, diffuse_albedo.rgb, specular_albedo.rgb, 1);

    // emissive
    float3 T0 = 0;

    // direct
    float3 T1 = f * Li * attenuation * NoL;

    if (is_real)
    {
        T1 = kinect_background.rgb;
        f = brdf(L, V, N, diffuse_albedo.rgb, 0, 1);
    }

    // indirect bounce
    float4 indirect_delta;
    float4 indirect_rho;

    indirect_delta = gi_from_lpv(P, N);
    indirect_rho   = gi_from_lpv(P, N);

    float3 T2 = 0;

    if (!is_real)
    {
        T2 += indirect_rho * gi_scale * diffuse_albedo.rgb / M_PI;
    }
    else
    {
        T2 += indirect_delta * gi_scale * diffuse_albedo;
    }

    if (debug_gi)
        return float4(abs(T2), 1.0f);

    return float4(max(T0 + T1 + T2, 0.0f), !is_real);
}
