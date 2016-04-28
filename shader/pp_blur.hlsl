/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

// Performs a gaussian blur in one direction
float4 gauss(in PS_INPUT input, in float2 tex_scale, in float sigma)
{
    float4 color = 0;

    float w,h;
    frontbuffer.GetDimensions(w,h);

    for (int i = -6; i < 6; i++)
    {
        float weight = gauss_weight(i, sigma);

        // compute tap tc
        float2 tc = input.tex_coord;
        tc += (i / float2(w,h)) * tex_scale;

        // fetch tap
        float4 tap = frontbuffer.Sample(ShadowFilter, tc);
        color += tap * weight;
    }

    return color;
}

float4 bilateral_gauss_far(in PS_INPUT input, in float2 tex_scale, in float sigma)
{
    float4 color = 0;

    float w,h;
    frontbuffer.GetDimensions(w,h);

    float4 center_tap_color = frontbuffer_full.Sample(StandardFilter, input.tex_coord);
    float center_tap_depth = rt_lineardepth.Sample(ShadowFilter, input.tex_coord).x;

    float dw,dh;
    rt_lineardepth.GetDimensions(dw,dh);

    float weightaccum = 0;

    for (int i = -6; i < 6; i++)
    {
        float weight = gauss_weight(i, sigma);

        // compute tap tc
        float2 tc = input.tex_coord;
        tc += (i / float2(w,h)) * tex_scale;

        float2 dtc = input.tex_coord;
        dtc += (i / float2(dw,dh)) * tex_scale;

        // fetch tap
        float4 tap = frontbuffer.Sample(ShadowFilter, tc);
        float dtap = rt_lineardepth.Sample(ShadowFilter, dtc).x;

        if (dtap >= center_tap_depth)
            color += tap * weight;
        else
            color += center_tap_color * weight;

        weightaccum += weight;
    }

    return float4(color.rgb, 1);
}

float4 bilateral_gauss_near(in PS_INPUT input, float2 tex_scale, float sigma)
{
    float4 accum = 0;
    float accum_weight = 0;

    float w,h;
    frontbuffer.GetDimensions(w,h);

    float4 center_tap_color = frontbuffer.Sample(StandardFilter, input.tex_coord);
    float center_depth_tap = rt_lineardepth.Sample(ShadowFilter, input.tex_coord).x;

    float dw,dh;
    rt_lineardepth.GetDimensions(dw,dh);

    [unroll]
    for (int i = -6; i < 6; i++)
    {
        // compute tap tc
        float2 tc = input.tex_coord;
        tc += (i / float2(w,h)) * tex_scale;

        float2 dtc = input.tex_coord;
        dtc += (i / float2(dw,dh)) * tex_scale;

        // fetch tap
        float4 tap = frontbuffer.Sample(ShadowFilter, tc);
        float depth_tap = rt_lineardepth.Sample(ShadowFilter, dtc).x;

        float depth_diff = abs(depth_tap - center_depth_tap);
        float r2 = depth_diff * 100.0 / center_depth_tap;
        float g = exp(-r2 * r2);

        if (depth_diff == 0)
            g = 1;

        float weight = gauss_weight(i, sigma) * g;

        if (depth_tap < center_depth_tap)
            accum += tap * weight;
        else
            accum += center_tap_color * weight;

        accum_weight += weight;
    }

    return float4(accum.rgb/accum_weight, 1);
}

float4 ps_gauss_bloom_h(in PS_INPUT inp) : SV_TARGET
{
    return gauss(inp, float2(0, 1), bloom_sigma);
}

float4 ps_gauss_bloom_v(in PS_INPUT inp) : SV_TARGET
{
    return gauss(inp, float2(1, 0), bloom_sigma);
}

float4 ps_gauss_dof_h(in PS_INPUT inp) : SV_TARGET
{
    return bilateral_gauss_far(inp, float2(0, 1), dof_coc_scale);
}

float4 ps_gauss_dof_v(in PS_INPUT inp) : SV_TARGET
{
    return bilateral_gauss_far(inp, float2(1, 0), dof_coc_scale);
}

float4 ps_gauss_godrays_h(in PS_INPUT inp) : SV_TARGET
{
    return bilateral_gauss_near(inp, float2(0, 1), 2.5);
}

float4 ps_gauss_godrays_v(in PS_INPUT inp) : SV_TARGET
{
    return bilateral_gauss_near(inp, float2(1, 0), 2.5);
}

float4 ps_copy(in PS_INPUT inp) : SV_TARGET
{
    return frontbuffer.SampleLevel(ShadowFilter, inp.tex_coord, 0);
}
