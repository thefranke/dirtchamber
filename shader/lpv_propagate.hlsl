/* 
 * lpv_propagate.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"
#include "tools.hlsl"

struct VS_LPV_PROPAGATE
{
    float4 pos                  : POSITION;
    float3 tex                  : TEXCOORD;
};

struct GS_LPV_PROPAGATE
{
    float4 pos                  : SV_Position;
    float3 tex                  : TEXCOORD;
    uint rtindex                : SV_RenderTargetArrayIndex;
};

struct PS_LPV_PROPAGATE
{
    float4 coeff_r              : SV_Target0;
    float4 coeff_g              : SV_Target1;
    float4 coeff_b              : SV_Target2;

    float4 coeff_acc_r          : SV_Target3;
    float4 coeff_acc_g          : SV_Target4;
    float4 coeff_acc_b          : SV_Target5;
};

cbuffer gi_parameters_ps        : register(b3)
{
	float vpl_scale			    : packoffset(c0.x);
	float lpv_flux_amplifier    : packoffset(c0.y);
	uint num_vpls			    : packoffset(c0.z);
	bool debug_gi			    : packoffset(c0.w);
}

cbuffer propagation_ps          : register(b13)
{
    uint iteration              : packoffset(c0.x);
    float3 pad                  : packoffset(c0.y);
}

Texture2DArray lpv_sh_r         : register(t7);
Texture2DArray lpv_sh_g         : register(t8);
Texture2DArray lpv_sh_b         : register(t9);

VS_LPV_PROPAGATE vs_lpv_propagate(in float3 pos : POSITION, float3 tex : TEXCOORD)
{
    VS_LPV_PROPAGATE output;
    
    output.pos = float4(pos.x, pos.y, pos.z, 1.0f);
    output.tex = tex;

    return output;
}

[maxvertexcount (3)]
void gs_lpv_propagate(in triangle VS_LPV_PROPAGATE input[3], inout TriangleStream<GS_LPV_PROPAGATE> stream)
{
    for(int v = 0; v < 3; ++v)
    {   
        GS_LPV_PROPAGATE output;
        output.rtindex      = input[v].tex.z;
        output.pos          = input[v].pos;
        output.tex          = input[v].tex;
        stream.Append(output);
    }

    stream.RestartStrip();
}

PS_LPV_PROPAGATE ps_lpv_propagate(in GS_LPV_PROPAGATE input)
{
    PS_LPV_PROPAGATE output;

    float famp = lpv_flux_amplifier;

    int3 lpv_pos = int3(input.pos.x, input.pos.y, input.tex.z);

    float3 offsets[6];
    offsets[0] = float3(0, 0, 1);
    offsets[1] = float3(1, 0, 0);
    offsets[2] = float3(0, 0,-1);
    offsets[3] = float3(-1,0, 0);
    offsets[4] = float3(0, 1, 0);
    offsets[5] = float3(0,-1, 0);

    float4 face_coeffs[6];
    face_coeffs[0] = sh_clamped_cos_coeff(offsets[0]);
    face_coeffs[1] = sh_clamped_cos_coeff(offsets[1]);
    face_coeffs[2] = sh_clamped_cos_coeff(offsets[2]);
    face_coeffs[3] = sh_clamped_cos_coeff(offsets[3]);
    face_coeffs[4] = sh_clamped_cos_coeff(offsets[4]);
    face_coeffs[5] = sh_clamped_cos_coeff(offsets[5]);

    float4 new_sh_r = float4(0,0,0,0);
    float4 new_sh_g = float4(0,0,0,0);
    float4 new_sh_b = float4(0,0,0,0);

    // add own contribution
    float4 ppos = float4(lpv_pos, 0);
    
    // add adjecant contribution
    for(int neighbor = 0; neighbor < 6; neighbor++)
    {
		float3 neighbor_offset = offsets[neighbor];

        //load the light value in the neighbor cell
        ppos = float4(lpv_pos + neighbor_offset, 0);

        // read from lpv
        float4 old_sh_r = lpv_sh_r.Load(ppos);
        float4 old_sh_g = lpv_sh_g.Load(ppos);
        float4 old_sh_b = lpv_sh_b.Load(ppos);
        
        // add up new incoming flux from surrounding nodes
        for(int face = 0; face < 6; face++)
        {
			float3 face_pos = offsets[face]*0.5f;
		
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
			float3 dir = face_pos - neighbor_offset;
            float len = length(dir);
			dir /= len;
			
			float solid_angle = 0;
			
            if (len <= 0.5)
                solid_angle = 0;
            else
                solid_angle = len >= 1.5f ? 22.95668f/(4*180.0f) : 24.26083f/(4*180.0f);

			float4 dir_sh = sh4(dir);

            float r = famp * solid_angle * dot(old_sh_r, dir_sh);
            float g = famp * solid_angle * dot(old_sh_g, dir_sh);
            float b = famp * solid_angle * dot(old_sh_b, dir_sh);
                            
            float4 coeffs = face_coeffs[face];

			r = max(0, r);
            g = max(0, g);
            b = max(0, b);

            new_sh_r += r * coeffs;
            new_sh_g += g * coeffs;
            new_sh_b += b * coeffs;
        }
    }

    if (iteration == 0)
    {
        new_sh_r += (lpv_sh_r.Load(ppos)) * famp;
        new_sh_g += (lpv_sh_g.Load(ppos)) * famp;
        new_sh_b += (lpv_sh_b.Load(ppos)) * famp;
    }

    output.coeff_r = new_sh_r;
    output.coeff_g = new_sh_g;
    output.coeff_b = new_sh_b;
    
    output.coeff_acc_r = new_sh_r;
    output.coeff_acc_g = new_sh_g;
    output.coeff_acc_b = new_sh_b;
    
    return output;
}

PS_LPV_PROPAGATE ps_delta_lpv_propagate(in GS_LPV_PROPAGATE input)
{
    PS_LPV_PROPAGATE output;

    float famp = lpv_flux_amplifier;

    int3 lpv_pos = int3(input.pos.x, input.pos.y, input.tex.z);

    float3 offsets[6];
    offsets[0] = float3(0, 0, 1);
    offsets[1] = float3(1, 0, 0);
    offsets[2] = float3(0, 0,-1);
    offsets[3] = float3(-1,0, 0);
    offsets[4] = float3(0, 1, 0);
    offsets[5] = float3(0,-1, 0);

    float4 face_coeffs[6];
    face_coeffs[0] = sh_clamped_cos_coeff(offsets[0]);
    face_coeffs[1] = sh_clamped_cos_coeff(offsets[1]);
    face_coeffs[2] = sh_clamped_cos_coeff(offsets[2]);
    face_coeffs[3] = sh_clamped_cos_coeff(offsets[3]);
    face_coeffs[4] = sh_clamped_cos_coeff(offsets[4]);
    face_coeffs[5] = sh_clamped_cos_coeff(offsets[5]);

    float4 new_sh_r = float4(0,0,0,0);
    float4 new_sh_g = float4(0,0,0,0);
    float4 new_sh_b = float4(0,0,0,0);

    // add own contribution
    float4 ppos = float4(lpv_pos, 0);
    
    // add adjecant contribution
    for(int neighbor = 0; neighbor < 6; neighbor++)
    {
		float3 neighbor_offset = offsets[neighbor];

        //load the light value in the neighbor cell
        ppos = float4(lpv_pos + neighbor_offset, 0);

        // read from lpv
        float4 old_sh_r = lpv_sh_r.Load(ppos);
        float4 old_sh_g = lpv_sh_g.Load(ppos);
        float4 old_sh_b = lpv_sh_b.Load(ppos);
        
        // add up new incoming flux from surrounding nodes
        for(int face = 0; face < 6; face++)
        {
			float3 face_pos = offsets[face]*0.5f;
		
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
			float3 dir = face_pos - neighbor_offset;
            float len = length(dir);
			dir /= len;
			
			float solid_angle = 0;
			
            if (len <= 0.5)
                solid_angle = 0;
            else
                solid_angle = len >= 1.5f ? 22.95668f/(4*180.0f) : 24.26083f/(4*180.0f);

			float4 dir_sh = sh4(dir);

            float d = 0.0001;

            float rr = dot(old_sh_r, dir_sh);
            // TODO: +2ms if you uncomment me :(
            //if (abs(rr) < d) rr = 0;
            float r = famp * solid_angle * rr;

            float rg = dot(old_sh_g, dir_sh);
            // TODO: +2ms if you uncomment me :(
            //if (abs(rg) < d) rg = 0;
            float g = famp * solid_angle * rg;

            float rb = dot(old_sh_b, dir_sh);
            // TODO: +2ms if you uncomment me :(
            //if (abs(rb) < d) rb = 0;
            float b = famp * solid_angle * rb;
           
            float4 coeffs = face_coeffs[face];

            new_sh_r += r * coeffs;
            new_sh_g += g * coeffs;
            new_sh_b += b * coeffs;
        }
    }
    
    output.coeff_r = new_sh_r;
    output.coeff_g = new_sh_g;
    output.coeff_b = new_sh_b;
    
    output.coeff_acc_r = new_sh_r;
    output.coeff_acc_g = new_sh_g;
    output.coeff_acc_b = new_sh_b;
    
    return output;
}