/* 
 * tools.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2011
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

struct directional_light
{
    float4 position;
    float4 normal;
    float4 color;
};

float gauss_weight(in int sample_dist, in float sigma)
{
	float g = 1.0f / sqrt(2.0f * 3.14159 * sigma * sigma);
	return (g * exp(-(sample_dist * sample_dist) / (2 * sigma * sigma)));
}

// Calculate the luminance of a color c
float luminance(in float3 c)
{
	return dot(c, float3(0.3, 0.59, 0.11));
}

// max[v1.v2]
float sdot(in float3 v1, in float3 v2)
{
    return saturate(dot(v1, v2));
}

// Transform a texture coordinate from [0, 1] to [-1, 1]
float2 to_proj(in float2 tc)
{
    return (tc - 0.5) * float2(2.0, -2.0);
}

// Transform a texture coordinate from [-1, 1] to [0, 1]
float2 to_tex(in float2 tc)
{
    return (tc + 1.0) * float2(0.5, -0.5);
}

// Transform a texture coordinate and an inverste projection matrix to a ray through the coordinate into the scene
float4 to_ray(in float2 tc, in float4x4 inv_mat)
{
    return mul(inv_mat, float4(to_proj(tc), 1.0, 1.0));
}

// First two bands of SH
float4 sh4(in float3 xyz)
{
    return float4(
         0.282094792,     
        -0.4886025119 * xyz.y,
         0.4886025119 * xyz.z,
        -0.4886025119 * xyz.x
    );
}

/* 
 * Clamped cosine oriented in z direction expressed in zonal harmonics and rotated to direction 'd'
 * zonal harmonics of clamped cosine in z:
 *     l=0 : A0 = 0.5*sqrt(PI)
 *     l=1 : A1 = sqrt(PI/3)
 *     l=2 : A2 = (PI/4)*sqrt(5/(4*PI))
 *     l=3 : A3 = 0
 *   to rotate zonal harmonics in direction 'd' : sqrt( (4*PI)/(2*l+1)) * zl * SH coefficients in direction 'd'
 *     l=0 : PI * SH coefficients in direction 'd' 
 *     l=1 : 2*PI/3 * SH coefficients in direction 'd' 
 *     l=2 : PI/4 * SH coefficients in direction 'd' 
 *     l=3 : 0
 */
float4 sh_clamped_cos_coeff(in float3 xyz)
{
    float4 v = sh4(xyz);
    float d = ((2.0*M_PI)/3.0f);

    return float4(
        M_PI * v.x,
        d * v.y,
        d * v.z,
        d * v.w
    );
}

// Percentage Closer Filtering
float pcf(in float2 tc, in float z, in Texture2D linear_shadowmap)
{
    SamplerComparisonState cmp_sampler
    {
        // sampler state
        Filter = COMPARISON_MIN_MAG_MIP_LINEAR;
        AddressU = MIRROR;
        AddressV = MIRROR;

        // sampler comparison state
        ComparisonFunc = GREATER;
    };

    const uint NUM_TAPS = 9;

    int2 offsets[NUM_TAPS] =
    {
        int2( 0,  0),
        int2( 1,  0),
        int2( 0,  1),
        int2(-1,  0),
        int2( 0, -1),
        int2( 1,  1),
        int2( 1, -1),
        int2(-1,  1),
        int2(-1, -1)
    };

    float ret = 0;

    for (uint f = 0; f < NUM_TAPS; ++f)
    {
        float4 fourtaps = linear_shadowmap.GatherCmpRed(cmp_sampler, tc, z, offsets[f]) - z;

        fourtaps = any(fourtaps > 0);

        ret += dot(fourtaps, fourtaps) * 0.1;
    }
      
    return ret / NUM_TAPS;
}

// Linear stepping function
float linear_step(in float mins, in float maxs, in float val)
{  
    return clamp((val - mins) / (maxs - mins), 0, 1);
}

// Helper function to reduce light bleeding in VSMs
float reduce_light_bleeding(in float p_max, in float amount)
{  
    return linear_step(amount, 1, p_max);  
}

// Percentage Closer Softshadows
float2 pcss(in float2 tc, in float z, in Texture2D linear_shadowmap, in SamplerState shadow_filter)
{
    float dlol = z/1200;
    return linear_shadowmap.SampleLevel(shadow_filter, tc, dlol).xy;
}

// Variance Shadow Mapping
float vsm(in float2 tc, in float z, in Texture2D linear_shadowmap, in SamplerState shadow_filter)
{
    // compute chebyshev upper bound
    float2 moments = pcss(tc, z, linear_shadowmap, shadow_filter);

    float lit = 0.0f;
    float e_x2 = moments.y;
    float ex_2 = moments.x * moments.x;
    float variance = e_x2 - ex_2;
    float md = moments.x - z;
    float md_2 = md * md;
    float p = variance / (variance + md_2);
    lit = max(p, z <= moments.x);

    return reduce_light_bleeding(lit, 0.5);
}

// Generate a VPL from a texture coordinate, an inverse light projecton matrix, a light position, and a GBuffer of the RSM
directional_light gen_vpl(in float2 tc, in float4x4 light_vp_inv, in float4 light_pos, in Texture2D colors, in Texture2D normals, in Texture2D linear_depth, in SamplerState vpl_filter)
{   
    directional_light vpl;

    float4 vcol = colors.SampleLevel(vpl_filter, tc, 0);
    float3 vnormal = normals.SampleLevel(vpl_filter, tc, 0).xyz * 2.0 - 1.0;
    float  vdepth = linear_depth.SampleLevel(vpl_filter, tc, 0).r;
    
    float4 dir = to_ray(tc, light_vp_inv);

    vpl.color    = vcol;
    vpl.normal   = float4(vnormal, 0);
    vpl.position = float4((light_pos + (normalize(dir) * vdepth)).xyz, 1);

    // mark invalid vpls with 0-length normal
    if (vdepth < 0.0001f)
    {
        vpl.color  = float4(0,0,0,0);
        vpl.normal = float4(0,0,0,0);
    }

    return vpl;
}

// Helper function to check if a VPL is valid
bool is_invalid(in directional_light vpl)
{
    return (vpl.normal.x == 0 && vpl.normal.y == 0 && vpl.normal.z == 0);
}

// Helper function for Hammersley sequences
float radical_inverse(in uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10;
}

// Hammersly sequence
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley2d(in uint i, in uint N)
{
    return float2(float(i)/float(N), radical_inverse(i));
}
