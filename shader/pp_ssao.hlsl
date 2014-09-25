/* 
 * SSAO effect by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

float ssao(in PS_INPUT inp)
{
    float2 tc = inp.tex_coord;
    float depth  = rt_lineardepth.Sample(StandardFilter, tc).r;
	float3 pos = camera_pos + (normalize(inp.view_ray.xyz) * depth);
    float3 normal = rt_normals.Sample(StandardFilter, tc).rgb * 2.0 - 1.0;

	if (ssao_scale == 0.0)
		return 1.0;

	float w, h;
	rt_lineardepth.GetDimensions(w, h);
	
	const float2 scale = float2(1.0/w, 1.0/h);
	
	float ret = 0.0;
	
	float2 samples[24] = {
		float2(1,1),
		float2(-1,1),
		float2(1,-1),
		float2(-1,-1),
		float2(1,0),
		float2(-1,0),
		float2(0,-1),
		float2(0,-1),

        float2(2, 1),
        float2(-2, 1),
        float2(2, -1),
        float2(-2, -1),
        float2(2, 0),
        float2(-2, 0),
        float2(0, -2),
        float2(0, -2),

        float2(1, 2),
        float2(-1, 2),
        float2(1, -2),
        float2(-1, -2),

        float2(2, 2),
        float2(-2, 2),
        float2(2, -2),
        float2(-2, -2),
	};
	
	[unroll]
    for (int i = 0; i < 24; ++i)
    {
		// TODO: Needs distance scaling
		float2 ntc = tc + samples[i] * scale * 2 * abs(simple_noise(tc));
		
		float4 occvr = to_ray(ntc, vp_inv);

		float occdepth = rt_lineardepth.Sample(StandardFilter, ntc.xy).r;
		
        // bilateral filter
        if (abs(occdepth - depth) > 0.1)
            continue;
        
        float3 occnorm = rt_normals.Sample(StandardFilter, ntc.xy).xyz * 2.0 - 1.0;
		float3 occpos = camera_pos + (normalize(occvr).xyz * occdepth);
		
		float3 diff = occpos - pos;
		float3 v = normalize(diff);
		float ddiff = length(diff);
		
        if (dot(occnorm, normal) < 0.95)
            ret += ssao_scale * saturate(dot(normal, v)) * (1.0 / (1.0 + ddiff));
    }

    return 1.0 - saturate(ret/SSAO_SAMPLES);
}

float4 ps_ssao(in PS_INPUT inp) : SV_TARGET
{
    float4 color = frontbuffer.Sample(StandardFilter, inp.tex_coord, 0);
    
    if (ssao_enabled && color.a > 0) 
        return color * ssao(inp);
    else
        return color;
}
