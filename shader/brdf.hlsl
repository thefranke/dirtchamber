/* 
 * brdf.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

#ifndef BRDF_HLSL
#define BRDF_HLSL
 
#define F0_WATER        0.02,0.02,0.02
#define F0_PLASTIC      0.03,0.03,0.03
#define F0_PLASTIC_HIGH 0.05,0.05,0.05
#define F0_GLASS        0.08,0.08,0.08
#define F0_DIAMOND      0.17,0.17,0.17
#define F0_IRON         0.56,0.57,0.58
#define F0_COPPER       0.95,0.64,0.54
#define F0_GOLD         1.00,0.71,0.29
#define F0_ALUMINIUM    0.91,0.92,0.92
#define F0_SILVER       0.95,0.93,0.88

float sqr(in float x) 
{ 
    return x * x; 
}

// Schlick's approximation of the fresnel term
float F_schlick(in float f0, in float LoH)
{
    // only have specular if f0 isn't 0
    //float enable = float(dot(f0, 1.0f) > 0.0f);
    return (f0 + (1.0f - f0) * pow(1.0f - LoH, 5.0f));
}

// Optimizied Schlick
// http://seblagarde.wordpress.com/2011/08/17/hello-world/
float SphericalGaussianApprox(in float CosX, in float ModifiedSpecularPower)
{
    return exp2(ModifiedSpecularPower * CosX - ModifiedSpecularPower);
}

#define OneOnLN2_x6 8.656170 // == 1/ln(2) * 6   (6 is SpecularPower of 5 + 1)

float3 F_schlick_opt(in float3 SpecularColor, in float3 E, in float3 H)
{
    // In this case SphericalGaussianApprox(1.0f - saturate(dot(E, H)), OneOnLN2_x6) is equal to exp2(-OneOnLN2_x6 * x)
    return SpecularColor + (1.0f - SpecularColor) * exp2(-OneOnLN2_x6 * saturate(dot(E, H)));
}

// Microfacet Models for Refraction through Rough Surfaces
// Walter et al.
// http://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
// aka Towbridge-Reitz
float D_ggx(in float alpha, in float NoH)
{
    float a2 = alpha*alpha;
    float cos2 = NoH*NoH;

    return (1.0f/M_PI) * sqr(alpha/(cos2 * (a2 - 1) + 1));

    /*
    // version from the paper, eq 33
    float CosSquared = NoH*NoH;
    float TanSquared = (1.0f - CosSquared)/CosSquared;
    return (1.0f/M_PI) * sqr(alpha/(CosSquared * (alpha*alpha + TanSquared)));
    */
}

// Smith GGX with denominator
// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
float G_smith_ggx(in float a, in float NoV, in float NoL)
{
    float a2 = a*a;
    float G_V = NoV + sqrt((NoV - NoV * a2) * NoV + a2);
    float G_L = NoL + sqrt((NoL - NoL * a2) * NoL + a2);
    return rcp(G_V * G_L);
}

// Schlick GGX
// http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html 
float G_UE4(in float alpha, in float NoV)
{
    float k = alpha/2;
    return NoV/(NoV * (1 - k) + k);
}

float G_implicit(in float NoV, in float NoL)
{
    return NoL * NoV;
}

// Beckmann distribution
float D_beckmann(in float m, in float t)
{
    float M = m*m;
    float T = t*t;
    return exp((T-1)/(M*T)) / (M*T*T);
}

// Helper to convert roughness to Phong specular power
float alpha_to_spec_pow(in float a) 
{
    return 2.0f / (a * a) - 2.0f;
}

// Helper to convert Phong specular power to alpha
float spec_pow_to_alpha(in float s) 
{
    return sqrt(2.0f / (s + 2.0f));
}

// Blinn Phong with conversion functions for roughness
float D_blinn_phong(in float n, in float NoH)
{
    float alpha = spec_pow_to_alpha(n);
    
    return (1.0f / (M_PI*alpha*alpha)) * pow(NoH, n); 
}

// Cook-Torrance specular BRDF + diffuse
float3 brdf(in float3 L, in float3 V, in float3 N, in float3 cdiff, in float3 cspec, in float roughness)
{
    float alpha = roughness*roughness;

    float3 H = normalize(L+V);
    
    float NoL = dot(N, L);
    float NoV = dot(N, V);
    float NoH = dot(N, H);
    float LoH = dot(L, H);
    
    // refractive index
    float n = 1.5; 
    float f0 = pow((1 - n)/(1 + n), 2);
    
    // the fresnel term
    float F = F_schlick(f0, LoH);

    // the geometry term
    float G = G_UE4(alpha, NoV);
    
    // the NDF term
    float D = D_ggx(alpha, NoH);

    // specular term
    float3 Rs = cspec/M_PI * 
                (F * G * D)/
                (4 * NoL * NoV);

    // diffuse fresnel, can be cheaper as 1-f0
    float Fd = F_schlick(f0, NoL);
    
    float3 Rd = cdiff/M_PI * (1.0f - Fd);

    return (Rd + Rs);
}

#endif
