/* 
 * CRT monitor effect by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

// Reference: https://www.shadertoy.com/view/MsXGD4

#include "postprocessing.hlsl"

float4 scanline(in float2 coord, in float4 screen)
{
	screen.rgb -= sin((coord.y * 0.7 - (time * 19.0))) * 0.002;
	screen.rgb -= sin((coord.y * 0.02 + (time * 6.0))) * 0.005;
	return screen;
}

float4 texture_chromatic_distort(in float2 coord)
{
	float4 frag;
	frag.r = frontbuffer.SampleLevel(StandardFilter, float2(coord.x - 0.003 * sin(time), coord.y), 0.0).r;
    frag.g = frontbuffer.SampleLevel(StandardFilter, float2(coord.x, coord.y), 0.0).g;
    frag.b = frontbuffer.SampleLevel(StandardFilter, float2(coord.x + 0.003 * sin(time), coord.y), 0.0).b;
    frag.a = frontbuffer.SampleLevel(StandardFilter, float2(coord.x, coord.y), 0.0).a;
	return frag;
}

float2 crt_distort(in float2 coord, in float bend)
{
	// put in symmetrical coords
	coord = (coord - 0.5) * 2.0;

	// deform coords
	coord.x *= 1.0 + pow(abs(abs(coord.y) / bend), 2.0);
	coord.y *= 1.0 + pow(abs(abs(coord.x) / bend), 2.0);

	// transform back to 0.0 - 1.0 space
	coord  = (coord / 2.0) + 0.5;

	return coord;
}

float vignette(in float2 uv)
{
	uv = (uv - 0.5) * 0.98;
	return clamp(pow(abs(cos(uv.x * M_PI)), 1.2) * pow(abs(cos(uv.y * M_PI)), 1.2) * 50.0, 0.0, 1.0);
}

float noise(in float2 uv)
{
    float a = noise_tex.SampleLevel(StandardFilter, float3(uv + time*6.0, 0.0), 0.0).r;
    float b = noise_tex.SampleLevel(StandardFilter, float3(uv + time*4.0, 0.5), 0.0).r;

    return clamp(a + b, 0.94, 1.0);
}

float4 ps_crt(in PS_INPUT inp) : SV_Target
{
    if (!crt_enabled)
        return frontbuffer.Sample(StandardFilter, inp.tex_coord);

    float2 uv = inp.tex_coord.xy;
	float2 crt_uv = crt_distort(uv, 3.2);

	float4 color = texture_chromatic_distort(crt_uv);
	
    float2 dim;
    frontbuffer.GetDimensions(dim.x, dim.y);

	float2 screen_space = crt_uv * dim;
	color = scanline(screen_space, color);
	
	color = lerp(vignette(crt_uv) * color, color, 0.2);
	
	float n = noise(crt_uv*2);

    if (any(clamp(crt_uv, float2(0,0), float2(1,1)) - crt_uv))
        return float4(0,0,0,1);
    else
	    return color * n;
}
