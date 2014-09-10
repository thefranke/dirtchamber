/* 
 * lpv_tools.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

SamplerState LPVFilter      : register(s1);

Texture2DArray lpv_r        : register(t7);
Texture2DArray lpv_g        : register(t8);
Texture2DArray lpv_b        : register(t9);

cbuffer lpv_parameters      : register(b7)
{
    float4x4 world_to_lpv   : packoffset(c0);
    uint lpv_size           : packoffset(c4.x);
    float3 pad              : packoffset(c4.y);
}

void lpv_trilinear_lookup(in float3 lpv_pos, inout float4 sh_r_val, inout float4 sh_g_val, inout float4 sh_b_val,
                          in Texture2DArray lpvr, in Texture2DArray lpvg, in Texture2DArray lpvb, in int lpv_size, in SamplerState LPVFilter)
{
    float3 tc = float3(lpv_pos.x, lpv_pos.y, lpv_pos.z * lpv_size);

    int zl = floor(tc.z);
    int zh = min(zl + 1, lpv_size - 1);

    float inv_zh = tc.z - zl;
    float inv_zl = 1.0f - inv_zh;

    float3 tc_l = float3(tc.x, tc.y, zl);
    float3 tc_h = float3(tc.x, tc.y, zh);

    sh_r_val = inv_zl * lpvr.SampleLevel(LPVFilter, tc_l, 0) + inv_zh * lpvr.SampleLevel(LPVFilter, tc_h, 0);
    sh_g_val = inv_zl * lpvg.SampleLevel(LPVFilter, tc_l, 0) + inv_zh * lpvg.SampleLevel(LPVFilter, tc_h, 0);
    sh_b_val = inv_zl * lpvb.SampleLevel(LPVFilter, tc_l, 0) + inv_zh * lpvb.SampleLevel(LPVFilter, tc_h, 0);
}

float4 gi_from_lpv(in float3 pos, in float3 N)
{
    float4 indirect = float4(0.0, 0.0, 0.0, 1.0);

    float4 normal_sh = sh_clamped_cos_coeff(-N);
		
	float4 shcoeff_red   = float4(0,0,0,0);
	float4 shcoeff_green = float4(0,0,0,0);
	float4 shcoeff_blue  = float4(0,0,0,0);
		
    float3 lpv_pos = mul(world_to_lpv, float4(pos, 1)).xyz;
    float3 lpv_normal = normalize(mul(world_to_lpv, float4(N, 0))).xyz;
    
    // bias
    lpv_pos.z -= 0.5/LPV_SIZE;

    // ignore stuff outside the volume
    if (lpv_pos.x < 0 || lpv_pos.x > 1 ||
        lpv_pos.y < 0 || lpv_pos.y > 1 ||
        lpv_pos.z < 0 || lpv_pos.z > 1)
        return 0.f;
    
    lpv_trilinear_lookup(lpv_pos, shcoeff_red, shcoeff_green, shcoeff_blue, lpv_r, lpv_g, lpv_b, LPV_SIZE, LPVFilter);

    indirect.r = dot(shcoeff_red,   normal_sh)/M_PI;
    indirect.g = dot(shcoeff_green, normal_sh)/M_PI;
    indirect.b = dot(shcoeff_blue,  normal_sh)/M_PI;

    return indirect;
}