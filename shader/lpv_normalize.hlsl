/* 
 * lpv_normalize.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

struct PS_LPV_NORMALIZE
{
    float4 coeff_r                  : SV_Target0;
    float4 coeff_g                  : SV_Target1;
    float4 coeff_b                  : SV_Target2;
};

struct GS_LPV_PROPAGATE
{
    float4 pos                      : SV_Position;
    float3 tex                      : TEXCOORD;
    uint rtindex                    : SV_RenderTargetArrayIndex;
};

Texture2DArray lpv_sh_r             : register(t7);
Texture2DArray lpv_sh_g             : register(t8);
Texture2DArray lpv_sh_b             : register(t9);
Texture2DArray lpv_inject_counter   : register(t10);

PS_LPV_NORMALIZE ps_lpv_normalize(in GS_LPV_PROPAGATE input)
{
    PS_LPV_NORMALIZE output;

    int4 lpv_pos = int4(input.pos.x, input.pos.y, input.tex.z, 0);

    float num_lights = abs(lpv_inject_counter.Load(lpv_pos).r);

    float scale = 1.0;
    
    if (num_lights > 0)
        scale = 1.0/num_lights;

    float4 r = lpv_sh_r.Load(lpv_pos);
    float4 g = lpv_sh_g.Load(lpv_pos);
    float4 b = lpv_sh_b.Load(lpv_pos);

    output.coeff_r = r * scale;
    output.coeff_g = g * scale;
    output.coeff_b = b * scale;

    return output;
}
