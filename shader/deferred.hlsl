/* 
 * deferred.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"
#include "brdf.hlsl"

SamplerState StandardFilter			: register(s0);
SamplerState ShadowFilter			: register(s2);

struct PS_INPUT 
{
	float4 position					: SV_POSITION;
	float2 tex_coord				: TEXCOORD0;
	float3 view_ray					: TEXCOORD1;
};

cbuffer camera_ps                   : register(b1)
{
    float4x4 vp						: packoffset(c0);
    float4x4 vp_inv					: packoffset(c4);
    float3 camera_pos				: packoffset(c8);
    float z_far						: packoffset(c8.w);
}

cbuffer light_ps                    : register(b2)
{
	directional_light main_light	: packoffset(c0);
	float4x4 light_mvp				: packoffset(c3);
    float4x4 light_vp_inv			: packoffset(c7);
    float4x4 light_vp_tex			: packoffset(c11);
}

cbuffer onetime_ps                  : register(b6)
{
	float4 scene_dim_max			: packoffset(c0);
	float4 scene_dim_min			: packoffset(c1);
}

Texture2D rt_colors					: register(t2);
Texture2D rt_specular				: register(t3);
Texture2D rt_normals				: register(t4);
Texture2D rt_lineardepth			: register(t5);
Texture2D rt_rsm_lineardepth		: register(t6);

Texture3D<float> noise_tex			: register(t14);

float shadow_attenuation(in float3 pos, in float3 Ll, in Texture2D linear_shadowmap, in float min_s = 0.0, in float min_o = 0.0)
{
    float4 tct = mul(light_vp_tex, float4(pos, 1));
    float2 tc = tct.xy/tct.w;

    if (tc.x >= 1 || tc.y >= 1 || tc.y < 0 || tc.x < 0)
        return min_o;
		
    float z = length(Ll) * (1.0 - SHADOW_BIAS);

    return min(1.0, vsm(tc, z, linear_shadowmap, ShadowFilter) + min_s);
}

float get_spotlight(in float3 L, in float3 N)
{
    float3 from_light = -L;
    float3 spot_dir = normalize(main_light.normal.xyz);

    float falloff = 0.7;

    float alpha = dot(from_light, spot_dir);

    float attenuation_dist = 1.0;

    if (alpha < falloff)
        attenuation_dist = 1.0 - 10*abs(alpha - falloff);

    return sdot(N, -from_light) * saturate(attenuation_dist);
}

struct gbuffer
{
    float4 diffuse_albedo;
    float4 specular_albedo;
    float4 normal;
    float  depth;
    uint   shading_mode;
};

/*
void unpack_gbuffer(out float4 diffuse_albedo,
                    out float4 specular_albedo,
                    out float3 normal
                    out float  depth
                    out uint   shading_mode)*/
gbuffer unpack_gbuffer(in float2 tc)
{
    gbuffer gb;
    
    gb.diffuse_albedo  = rt_colors.Sample(StandardFilter, tc).rgba;
    gb.specular_albedo = rt_specular.Sample(StandardFilter, tc).rgba;
    gb.normal = rt_normals.Sample(StandardFilter, tc).xyzw;
    gb.depth = rt_lineardepth.Sample(StandardFilter, tc).r;
    gb.shading_mode = round(gb.normal.a * 2);
    
    return gb;
}
