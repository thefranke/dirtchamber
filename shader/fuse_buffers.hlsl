/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

SamplerState StandardFilter     : register(s0);

struct PS_INPUT
{
    float4 position             : SV_POSITION;
    float2 tex_coord            : TEXCOORD0;
    float3 view_ray             : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 color                : SV_Target0;
    float4 specular             : SV_Target1;
    float4 normal               : SV_Target2;
    float2 ldepth               : SV_Target3;
};

Texture2D rt_color              : register(t0);
Texture2D rt_specular           : register(t1);
Texture2D rt_normal             : register(t2);
Texture2D rt_lineardepth        : register(t3);
Texture2D rt_kinect_color       : register(t4);
Texture2D rt_kinect_depth       : register(t5);
Texture2D rt_tracked_scene      : register(t10);

cbuffer gi_parameters_ps        : register(b3)
{
    float vpl_scale             : packoffset(c0.x);
    float lpv_flux_amplifier    : packoffset(c0.y);
    uint num_vpls               : packoffset(c0.z);
    bool debug_gi               : packoffset(c0.w);
    float4x4 world_to_lpv       : packoffset(c1);
}

PS_OUTPUT ps_fuse_buffers(in PS_INPUT inp) : SV_TARGET
{
    PS_OUTPUT output;

    float2 vd = rt_lineardepth.Sample(StandardFilter, inp.tex_coord).rg;
    float virtual_depth = vd.r;

    float real_depth = rt_kinect_depth.Sample(StandardFilter, inp.tex_coord).r * 4 - 7;
    float2 rd = float2(real_depth, real_depth * real_depth);

    float4 virtual_color = rt_color.Sample(StandardFilter, inp.tex_coord);
    float4 real_color    = rt_kinect_color.Sample(StandardFilter, inp.tex_coord);

    real_color = pow(real_color, GAMMA);

    output.color = real_color;

#if 0
    output.ldepth = rd;
    output.normal = float4(0.0,1.0,0.0,0);
#else
    output.ldepth = float2(0,0);
    output.normal = float4(0.0,0.0,0.0,0);
#endif

    output.specular = float4(0,0,0,0);

    if (virtual_depth > 0)
    {
        float3 tracked = rt_tracked_scene.Sample(StandardFilter, inp.tex_coord).rgb;

        if (length(tracked) != 0 || debug_gi)
            output.color = real_color;
        else
            output.color = virtual_color;

        output.normal = rt_normal.Sample(StandardFilter, inp.tex_coord);

        output.ldepth = vd;
        output.specular = rt_specular.Sample(StandardFilter, inp.tex_coord);
    }

    return output;
}
