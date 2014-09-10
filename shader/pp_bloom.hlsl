/* 
 * Bloom effect by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"
#include "tonemapping.hlsl"

float average_luminance()
{
    return rt_adapted_luminance.Load(uint3(0,0,0)).x;
}

float ps_adapt_exposure(in PS_INPUT inp) : SV_Target
{   
    //get medium brightness of scene
	float curr_lum = luminance(frontbuffer.SampleLevel(StandardFilter, float2(0.5, 0.5), 10).rgb);
    float last_lum = rt_adapted_luminance.Load(uint3(0, 0, 0)).x;
	
    float v = last_lum + (curr_lum - last_lum) * (1 - exp(-time_delta * exposure_speed));
	
    if (!exposure_adapt)
        v = 0.5;
    
    return v;
}

// Determines the color based on exposure settings
float3 calc_exposed_color(in float3 color, in float average_lum, in float threshold, out float exposure)
{
	// Use geometric mean
	average_lum = max(average_lum, 0.001f);
	float keyValue = exposure_key;
	float linear_exposure = (exposure_key / average_lum);
	exposure = log2(max(linear_exposure, 0.0001f));
    exposure -= threshold;
    return exp2(exposure) * color;
}

// Applies exposure and tone mapping to the specific color, and applies
// the threshold to the exposure value.
float3 tone_map(in float4 color, in float average_lum, in float threshold, out float exposure)
{
    color.rgb = calc_exposed_color(color.rgb, average_lum, threshold, exposure);
    color.rgb = tonemap_uc2(color.rgb);

    return color.rgb;
}

float4 ps_bloom(in PS_INPUT inp) : SV_TARGET
{
    float4 color = frontbuffer.SampleLevel(StandardFilter, inp.tex_coord, 0);
    float avg_lum = average_luminance();

    float exposure = 0;
    color.rgb = tone_map(color.rgba, avg_lum, 0, exposure);
    
    float4 bloom = bloom_buffer.Sample(ShadowFilter, inp.tex_coord);

    if (bloom_enabled)
        return float4((color + bloom).rgb, color.a);
    else
        return color;
}

float4 ps_bloom_treshold(in PS_INPUT inp) : SV_TARGET
{
    float4 color = frontbuffer.Sample(StandardFilter, inp.tex_coord);

    float avg_lum = average_luminance();
    float exposure = 0;
    color.rgb = calc_exposed_color(color.rgb, avg_lum, bloom_treshold, exposure);

    if (dot(color.rgb, 0.333f) <= 0.01f)
        color = 0.f;

    return float4(color.rgb, 1);
}
