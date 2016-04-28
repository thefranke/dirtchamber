/*
 * The Dirtchamber - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

SamplerState StandardFilter : register(s0);

struct VS_MESH_INPUT
{
    float3 pos              : POSITION;
    float3 norm             : NORMAL;
    float2 texcoord         : TEXCOORD0;
    float3 tangent          : TANGENT;
};

struct VS_MESH_OUTPUT
{
    float4 pos              : SV_POSITION;
    float4 norm             : NORMAL;
    float3 tangent          : TANGENT;
    float2 texcoord         : TEXCOORD0;
    float3 wpos             : WPOS;
};

struct PS_MESH_OUTPUT
{
    float4 color            : SV_Target0;
    float4 normal           : SV_Target1;
    float2 ldepth           : SV_Target2;
    float4 specular         : SV_Target3;
};

cbuffer camera_vs           : register(b0)
{
    float4x4 vp             : packoffset(c0);
    float4x4 vp_inv         : packoffset(c4);
    float3 camera_pos       : packoffset(c8);
    float pad0              : packoffset(c8.w);
}

cbuffer meshdata_vs         : register(b1)
{
    float4x4 world          : packoffset(c0);
}

cbuffer meshdata_ps         : register(b0)
{
    float4 diffuse_color    : packoffset(c0);
    bool has_diffuse_tex    : packoffset(c1.x);
    bool has_normal_tex     : packoffset(c1.y);
    bool has_specular_tex   : packoffset(c1.z);
    bool has_alpha_tex      : packoffset(c1.w);
}

cbuffer per_frame_ps        : register(b5)
{
    float time              : packoffset(c0.x);
    float time_delta        : packoffset(c0.y);
    float2 pad1             : packoffset(c0.z);
}

Texture2D diffuse_tex       : register(t0);
Texture2D normal_tex        : register(t1);
Texture2D specular_tex      : register(t2);
Texture2D alpha_tex         : register(t3);

VS_MESH_OUTPUT vs_skydome(in VS_MESH_INPUT input)
{
    VS_MESH_OUTPUT output;

    // compensate for camera movement
    float4 pos = float4(input.pos + camera_pos, 1);

    output.pos = mul(vp, mul(world, pos));
    output.norm = float4(input.norm, 0);
    output.tangent = float3(0,0,0);
    output.texcoord = input.texcoord.xy;

    output.wpos = input.pos;

    return output;
}

PS_MESH_OUTPUT ps_skydome(in VS_MESH_OUTPUT input)
{
    PS_MESH_OUTPUT output;

    float2 tc = input.texcoord;
    tc.x += time * 0.001;

    float4 sky = pow(abs(diffuse_tex.Sample(StandardFilter, tc)), GAMMA);
    float4 cloudy_sky = sky;

    if (has_specular_tex)
    {
        tc.x += time*0.005;

        float clouds = pow(abs(specular_tex.Sample(StandardFilter, tc).r), GAMMA);
        cloudy_sky = lerp(sky, clouds, 0.8);
    }

    output.color = lerp(cloudy_sky, sky, tc.y);
    output.color.a = 0;

    output.normal = float4(0,0,0,0);
    output.ldepth = float2(0,0);
    output.specular = float4(0,0,0,0);

    return output;
}
