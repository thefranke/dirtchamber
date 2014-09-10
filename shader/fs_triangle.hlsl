/* 
 * fs_triangle.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

struct PS_INPUT 
{
	float4 position	    : SV_POSITION;
	float2 tex_coord    : TEXCOORD0;
	float3 view_ray	    : TEXCOORD1;
};

cbuffer camera_vs : register(b0)
{
    float4x4 vp         : packoffset(c0);
    float4x4 vp_inv     : packoffset(c4);
    float3 camera_pos   : packoffset(c8);
    float pad0          : packoffset(c8.w);
}

// Note: could do way easier with SV_VertexID, but stupid HLSL debugger won't have it
PS_INPUT vs_fs_triangle(in float3 pos : POSITION)
{
	PS_INPUT outp;

    float4 position = float4(pos, 1.f);
    
	outp.position  = position;
	outp.tex_coord = position.xy * float2(0.5, -0.5) + 0.5;
    outp.view_ray  = mul(vp_inv, position).xyz;

	return outp;
}
