/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

#define GODRAYS_MIST_SPEED  20
#define GODRAYS_NUM_SAMPLES 60
#define GODRAYS_NUM_SAMPLES_RCP 0.015625
#define GODRAYS_DITHERING   50

float shadowed(in float3 pos)
{
    float4 tct = mul(light_vp_tex, float4(pos, 1));
    float2 tc = tct.xy/tct.w;

    if (tc.x >= 1 || tc.y >= 1 || tc.y < 0 || tc.x < 0)
        return 0.0;

    float3 v = main_light.position.xyz - pos;

    float VoL = saturate(dot(normalize(-v), main_light.normal.xyz));

    float z = length(v);
    z *= 1.0 - SHADOW_BIAS;

    return vsm(tc, z, rt_rsm_lineardepth, ShadowFilter) * VoL;
}

float P(in float3 x, in float3 view_dir)
{
    float4 scene_delta = scene_dim_max - scene_dim_min;
    float sdiv = max(scene_delta.x, max(scene_delta.y, scene_delta.z));

    float t = time * GODRAYS_MIST_SPEED;
    float3 xx = x + float3(0, t, 0);
    float3 tcv = xx / (sdiv/5);

    float v = noise_tex.Sample(StandardFilter, tcv).r;

    // Schlick phase function
    /*
    float k = -0.1;
    float costheta = 1;
    float p = (1 - k*k) / (4 * M_PI*(1 + k*costheta*costheta));
    */

    return v*v;
}

// Volumetric Fogging 2
// http://www.humus.name/index.php?page=3D&ID=70
float4 godrays_humus(in PS_INPUT inp)
{
    float m = length(scene_dim_max - scene_dim_min);
    float s = rt_lineardepth.Sample(StandardFilter, inp.tex_coord).r;

    float3 view_dir = s * normalize(inp.view_ray.xyz) * GODRAYS_NUM_SAMPLES_RCP;

    float n = godrays_tau * length(view_dir);

    float a = 1.0;
    float b = 0.0;

    float3 pos = camera_pos;

    uint w,h;
    rt_lineardepth.GetDimensions(w, h);

    // dither shadow
    // THINK: maybe undo dithering if enough samples were fogged?
    float rf = noise_tex.Sample(StandardFilter, float3(inp.tex_coord*GODRAYS_DITHERING, 0));
    float rando = 1.0 + (2.0 * rf - 1.0) * s / m;

    //[unroll(GODRAYS_NUM_SAMPLES)]
    for (int i = 0; i < GODRAYS_NUM_SAMPLES; i++)
    {
        pos += view_dir * rando;

        float v = shadowed(pos);
        float fog = P(pos, view_dir);

        float x = 1.0 - fog * n;
        a *= x;
        b = lerp(v, b, x);
    }

    return b;
}

float4 ps_godrays(in PS_INPUT inp) : SV_TARGET
{
    float4 color = frontbuffer.Sample(StandardFilter, inp.tex_coord, 0);

    if (godrays_enabled)
        return color + godrays_humus(inp);
    else
        return color;
}

float4 ps_godrays_halfres(in PS_INPUT inp) : SV_TARGET
{
    if (godrays_enabled)
        return godrays_humus(inp);
    else
        return float4(0, 0, 0, 0);
}

float4 bilateral_sample(in float2 tc)
{
    float w, h;

    rt_lineardepth.GetDimensions(w, h);

    float center_tap = rt_lineardepth.SampleLevel(ShadowFilter, tc, 0).x;

    float2 dtc_rcp = float2(1.0 / w, 1.0 / h)*2;

    float min_depth_diff = 1.0;

    float2 nearest_sample = tc;

    for (int i = -1; i <= 1; ++i)
    for (int j = -1; j <= 1; ++j)
    {
        float2 stc = tc + float2(i, j)*dtc_rcp;
        float tap = rt_lineardepth.SampleLevel(ShadowFilter, stc, 1).x;

        float depth_diff = abs(center_tap - tap);

        if (depth_diff < min_depth_diff)
        {
            min_depth_diff = depth_diff;
            nearest_sample = stc;
        }
    }

    return frontbuffer_blurred.SampleLevel(StandardFilter, nearest_sample, 0);
}

float4 ps_godrays_merge(in PS_INPUT inp) : SV_TARGET
{
    float4 color = frontbuffer.SampleLevel(StandardFilter, inp.tex_coord, 0);
    float4 godrays = frontbuffer_blurred.SampleLevel(StandardFilter, inp.tex_coord, 0);

    if (godrays_enabled)
        return float4((color + godrays).rgb, color.a);
    else
        return color;
}
