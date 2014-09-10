#include "common.h"

SamplerState StandardFilter : register(s0);

struct VS_IN
{
	float3 pos						: POSITION;
	float3 norm						: NORMAL;
    float2 texcoord 				: TEXCOORD0;
	float3 tangent					: TANGENT;
};

struct VS_OUT
{
	float4 pos						: POSITION;
	float3 norm						: NORMAL;
	float2 texcoord					: TEXCOORD0;
};

struct GS_OUT
{
	float4 pos_sv					: SV_POSITION;
	float4 pos						: POSITION;
	float3 norm						: NORMAL;
	float2 texcoord					: TEXCOORD0;
	int axis						: AXIS;

};

struct PS_OUT
{
	float4 pos						: SV_Target0;
};

cbuffer meshdata_vs					: register(b1)
{
    float4x4 world					: packoffset(c0);
}

cbuffer parameters					: register(b7)
{
    float4x4 world_to_svo;
    float4 bb_min;
    float4 bb_max;
    float4 pad[2];
};

cbuffer meshdata_ps : register(b0)
{
    float4 diffuse_color            : packoffset(c0);
    float4 specular_color           : packoffset(c1);
    float4 emissive_color	        : packoffset(c2);
    bool has_diffuse_tex            : packoffset(c3.x);
    bool has_normal_tex             : packoffset(c3.y);
    bool has_specular_tex           : packoffset(c3.z);
    bool has_alpha_tex              : packoffset(c3.w);
    uint shading_mode               : packoffset(c4.x);
    float roughness                 : packoffset(c4.y);
    float refractive_index          : packoffset(c4.z);
}

Texture2D diffuse_tex               : register(t0);

RWTexture3D<float4> voxel_volume    : register(u1);
RWTexture3D<float4> v_rho           : register(u2);

VS_OUT vs_svo_voxelize(in VS_IN input)
{
	VS_OUT output = (VS_OUT) 0;
    
    output.pos = mul(world_to_svo, mul(world, float4(input.pos, 1.0)));
	output.norm = input.norm;
    output.texcoord = input.texcoord;
	
    return output;
}

[maxvertexcount(3)]
void gs_svo_voxelize(in triangle VS_OUT input[3], inout TriangleStream<GS_OUT> outputStream)
{
	GS_OUT output = (GS_OUT) 0;

	float4 scale = float4(2.0, 2.0, 1.0, 1.0);
	float4 shift = float4(-1.0, -1.0, 0.0, 0.0);

	float3 a = input[0].pos.xyz;
	float3 b = input[1].pos.xyz;
	float3 c = input[2].pos.xyz;

	float3 surface_norm = cross((b - a), (c - a));

	// invert values to point in positive axis direction
	float3 snormabs = abs(surface_norm);

	float dominant = max(snormabs.x, max(snormabs.y, snormabs.z));

	if(dominant == snormabs.z)
	{
		for (int i = 0; i < 3; i++)
		{
			output.axis = 2;
			output.pos = input[i].pos.xyzw * scale + shift;
			output.pos_sv = output.pos;
			output.norm = input[i].norm;
            output.texcoord = input[i].texcoord;
			outputStream.Append(output);
		}
	}
	else if(dominant == snormabs.y)
	{
		for (int i = 0; i < 3; i++)
		{
			output.axis = 1;
			output.pos = input[i].pos.xzyw * scale + shift;
			output.pos_sv = output.pos;
			output.norm = input[i].norm;
            output.texcoord = input[i].texcoord;
			outputStream.Append(output);
		}
	}
	else if(dominant == snormabs.x)
	{
		for (int i = 0; i < 3; i++)
		{
			output.axis = 0;
			output.pos = input[i].pos.zyxw * scale + shift;
			output.pos_sv = output.pos;
			output.norm = input[i].norm;
            output.texcoord = input[i].texcoord;
			outputStream.Append(output);
		}
	}
}

void splat(in uint3 pos, in float3 N, in float2 tc)
{
    float4 emissive = float4(0, 0, 0, 0);

    if (length(emissive_color.rgb) > 0.0)
    {
        // diffuse
        if (has_diffuse_tex)
            emissive = pow(abs(diffuse_tex.Sample(StandardFilter, tc)), GAMMA) * emissive_color;
        else
            emissive = diffuse_color + emissive_color;
    }
    
    v_rho[pos] = emissive;

    voxel_volume[pos] = float4(N*0.5+0.5, 0.5);
}

void ps_svo_voxelize(in GS_OUT input)
{
	float3 scale = float3(0.5, 0.5, 1.0);
	float3 shift = float3(1.0, 1.0, 0.0);

	if (any(input.pos.xyz < -1) || any(input.pos.xyz > 1)) return;

	// transform homogeneous coordinates to volume coordinates. (-1 < x < 1, -1 < y < 1, 0 < z < 1)
	uint3 pos = SVO_SIZE * scale * (input.pos.xyz + shift);
	
    if(input.axis == 2) // if z dominant
	{
		//do nothing
	}
	else if(input.axis == 1) // if y dominant
	{
		pos = pos.xzy;
	}
	else if(input.axis == 0) // if x dominant
	{
		pos = pos.zyx;
	}
	
    splat(pos, input.norm, input.texcoord);
}