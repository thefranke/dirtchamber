/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"

SamplerState StandardFilter            : register(s0);
SamplerState ShadowFilter            : register(s1);

struct PS_INPUT
{
    float4 position                    : SV_POSITION;
    float2 tex_coord                : TEXCOORD0;
    float3 view_ray                    : TEXCOORD1;
};

cbuffer onetime_ps : register(b6)
{
    float4 scene_dim_max            : packoffset(c0);
    float4 scene_dim_min            : packoffset(c1);;
}

cbuffer camera_ps : register(b1)
{
    float4x4 vp                        : packoffset(c0);
    float4x4 vp_inv                    : packoffset(c4);
    float3 camera_pos                : packoffset(c8);
    float z_far                        : packoffset(c8.w);
}

cbuffer light_ps : register(b2)
{
    directional_light main_light     : packoffset(c0);
    float4x4 light_vp                : packoffset(c3);
    float4x4 light_vp_inv            : packoffset(c7);
    float4x4 light_vp_tex            : packoffset(c11);
    float flux_scale                 : packoffset(c15);
}

cbuffer postprocessing_parameters_ps : register(b4)
{
    bool  ssao_enabled               : packoffset(c0.x);
    float ssao_scale                 : packoffset(c0.y);

    bool  godrays_enabled            : packoffset(c0.z);
    float godrays_tau                : packoffset(c0.w);

    bool  dof_enabled                : packoffset(c1.x);
    float dof_focal_plane            : packoffset(c1.y);
    float dof_coc_scale              : packoffset(c1.z);

    bool  bloom_enabled              : packoffset(c1.w);
    float bloom_sigma                : packoffset(c2.x);
    float bloom_treshold             : packoffset(c2.y);

    bool  fxaa_enabled               : packoffset(c2.z);

    bool  exposure_adapt             : packoffset(c2.w);
    float exposure_key               : packoffset(c3.x);
    float exposure_speed             : packoffset(c3.y);

    bool crt_enabled                 : packoffset(c3.z);
    bool film_grain_enabled          : packoffset(c3.w);
}

cbuffer per_frame_ps                 : register(b5)
{
    float time                       : packoffset(c0.x);
    float time_delta                 : packoffset(c0.y);
    float2 pad0                      : packoffset(c0.z);
}

Texture2D frontbuffer_full           : register(t0);

Texture2D rt_colors                  : register(t2);
Texture2D rt_specular                : register(t3);
Texture2D rt_normals                 : register(t4);
Texture2D rt_lineardepth             : register(t5);

Texture2D rt_rsm_lineardepth         : register(t6);

Texture2D frontbuffer                : register(t10);
Texture2D rt_adapted_luminance       : register(t11);
Texture2D bloom_buffer               : register(t12);
Texture2D frontbuffer_blurred        : register(t13);

Texture3D<float> noise_tex           : register(t14);
