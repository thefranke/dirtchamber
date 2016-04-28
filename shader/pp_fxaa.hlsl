/*
 * The Dirtchamber - Tobias Alexander Franke 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "postprocessing.hlsl"

#define FXAA_PC 1
#define FXAA_HLSL_4 1
#define FXAA_QUALITY__PRESET 12
#define FXAA_GREEN_AS_LUMA 1

#include "Fxaa3_11.h"

float4 ps_fxaa(in PS_INPUT inp) : SV_TARGET
{
    if (!fxaa_enabled)
        return frontbuffer.Sample(StandardFilter, inp.tex_coord);

    float4 unused = float4(0,0,0,0);

    FxaaFloat2 pos = inp.tex_coord;
    FxaaFloat4 fxaaConsolePosPos = unused;
    FxaaTex tex = { StandardFilter, frontbuffer };
    FxaaTex fxaaConsole360TexExpBiasNegOne = { StandardFilter, frontbuffer };
    FxaaTex fxaaConsole360TexExpBiasNegTwo = { StandardFilter, frontbuffer };

    float w,h;
    frontbuffer.GetDimensions(w,h);

    FxaaFloat2 fxaaQualityRcpFrame = float2(1.0/w, 1.0/h);

    FxaaFloat4 fxaaConsoleRcpFrameOpt = unused;
    FxaaFloat4 fxaaConsoleRcpFrameOpt2 = unused;
    FxaaFloat4 fxaaConsole360RcpFrameOpt2 = unused;
    FxaaFloat fxaaQualitySubpix = 0.75;
    FxaaFloat fxaaQualityEdgeThreshold = 0.166;
    FxaaFloat fxaaQualityEdgeThresholdMin = 0.0833;
    FxaaFloat fxaaConsoleEdgeSharpness = 8.0;
    FxaaFloat fxaaConsoleEdgeThreshold = 0.125;
    FxaaFloat fxaaConsoleEdgeThresholdMin = 0.05;
    FxaaFloat4 fxaaConsole360ConstDir = unused;

    return FxaaPixelShader(
        pos,
        fxaaConsolePosPos,
        tex,
        fxaaConsole360TexExpBiasNegOne,
        fxaaConsole360TexExpBiasNegTwo,
        fxaaQualityRcpFrame,
        fxaaConsoleRcpFrameOpt,
        fxaaConsoleRcpFrameOpt2,
        fxaaConsole360RcpFrameOpt2,
        fxaaQualitySubpix,
        fxaaQualityEdgeThreshold,
        fxaaQualityEdgeThresholdMin,
        fxaaConsoleEdgeSharpness,
        fxaaConsoleEdgeThreshold,
        fxaaConsoleEdgeThresholdMin,
        fxaaConsole360ConstDir
    );
}
