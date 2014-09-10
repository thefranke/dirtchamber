/*
 * Film grain effect by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

float grain(in float2 tc, in float3 value)
{
    const float intensity = 0.2;

    float l = luminance(value);

    // add noise
    float x = tc.x * tc.y * time *  1000.0;
    x = fmod(x, 13.0) * fmod(x, 123.0);
    float dx = fmod(x, 0.01);

    float y = clamp(0.1 + dx * 100.0, 0.0, 1.0) * intensity;
    float r = 0.5 * intensity - y;

    return 1.0 + r;
}

float4 ps_film_grain(in PS_INPUT inp) : SV_Target
{
    float4 value = frontbuffer.Sample(StandardFilter, inp.tex_coord);

    if (!film_grain_enabled)
        return value;
    else
    {
        float g = grain(inp.tex_coord, value.rgb);
        float3 ggg = float3(g, g, g);
        return float4(value.rgb * ggg, value.a);
    }
}
