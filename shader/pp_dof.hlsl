/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

float blur_factor(in float depth)
{
    return smoothstep(0, dof_focal_plane, abs(depth));
}

float4 ps_depth_of_field(in PS_INPUT inp) : SV_Target
{
    float4 blurred = frontbuffer_blurred.Sample(StandardFilter, inp.tex_coord);
    float4 full = frontbuffer.Sample(StandardFilter, inp.tex_coord);
    float depth = rt_lineardepth.Sample(StandardFilter, inp.tex_coord).x;

    if (!dof_enabled)
        return full;

    // TODO: a should be 0 or 1, but gets interpolated in between somewhere
    if (full.a < 0.5)
        return full;

    return lerp(full, blurred, saturate(blur_factor(depth)));
}
