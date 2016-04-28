/*
 * The Dirtchamber - Tobias Alexander Franke 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

SamplerState ShadowFilter   : register(s2);

struct PS_OVERLAY_INPUT
{
    float4 position         : SV_POSITION;
    float2 tex_coord        : TEXCOORD0;
};

Texture2D overlay_tex       : register(t10);

float2 calc_tex_coord(in float2 tex_coord)
{
    float s = 0.3;

    if (tex_coord.x >= s ||
        tex_coord.x <  0.0 ||
        tex_coord.y >= s ||
        tex_coord.x <  0.0)
        discard;

    return tex_coord * (1.0/s);
}

float4 ps_overlay(in PS_OVERLAY_INPUT In) : SV_Target
{
    float2 ntex = calc_tex_coord(In.tex_coord);

    float4 over = abs(overlay_tex.Sample(ShadowFilter, ntex));

    if (over.x > 1.0 || over.y > 1.0 || over.z > 1.0)
        over/=1000;

    return over;
}

float4 ps_overlay_depth(in PS_OVERLAY_INPUT In) : SV_TARGET
{
    float2 ntex = calc_tex_coord(In.tex_coord);

    float d = 1.0 - overlay_tex.Sample(ShadowFilter, ntex).r;

    return float4(d,d,d,1);
}
