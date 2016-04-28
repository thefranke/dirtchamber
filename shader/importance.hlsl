/*
 * The Dirtchamber - Tobias Alexander Franke 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#ifndef IMPORTANCE_HLSL
#define IMPORTANCE_HLSL

// Reference: Real Shading in Unreal Engine 4
// by Brian Karis
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float3 sample_to_world(in float phi, in float cos_theta, in float sin_theta, in float3 N)
{
    float3 H;

    H.x = sin_theta * cos(phi);
    H.y = sin_theta * sin(phi);
    H.z = cos_theta;

    float3 up_vec = abs(N.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
    float3 tangent_x = normalize(cross(up_vec, N));
    float3 tangent_y = cross(N, tangent_x);

    return tangent_x * H.x + tangent_y * H.y + N * H.z;
}

// Importance sample a GGX specular function
float3 importance_sample_ggx(in float2 xi, in float a, in float3 N)
{
    float phi = 2 * M_PI * xi.x;
    float cos_theta = sqrt((1 - xi.y)/(1 + (a*a - 1) * xi.y));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);

    return sample_to_world(phi, cos_theta, sin_theta, N);
}

// Importance sample a Phong specular function
float3 importance_sample_phong(in float2 xi, in float a, in float3 N)
{
    float n = alpha_to_spec_pow(a);

    float phi = 2 * M_PI * xi.x;
    float cos_theta = pow(xi.y, (1.0f/(n+1.0f)));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);

    return sample_to_world(phi, cos_theta, sin_theta, N);
}

// Randomly sample a specular function.
float3 random_sample(in float2 xi, in float a, in float3 N)
{
    float theta = M_PI * xi.y;
    float phi = 2 * M_PI * xi.x;

    float sin_theta = sin(theta);
    float cos_theta = cos(theta);

    float3 H = sample_to_world(phi, cos_theta, sin_theta, N);

    if (dot(H, N) < 0)
        H *=-1;

    return H;
}

// Compute a LOD level for filtered importance sampling.
// From GPU Gems 3: GPU-Based Importance Sampling.
float compute_lod(in float3 H, in float pdf, in uint num_samples, in uint ww, in uint hh)
{
    return max(0, 0.5*log2((ww*hh)/float(num_samples)) - 0.5*log2(pdf));
}

// Compute latlong coordinates for a 3D vector v.
float2 latlong(in float3 v)
{
    v = normalize(v);

    float theta = acos(v.y) + M_PI;

    float phi = atan2(v.z, v.x) + M_PI/2;

    return float2(phi, theta) * float2(.1591549, .6366198 / 2);
}

// Importance sample a specular GGX function from an environment map.
// From Real Shading in Unreal Engine 4
// by Brian Karis
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float3 specular_ibl_is(in float3 specular_albedo, in float roughness, in float3 N, in float3 V, in Texture2D env_map, in SamplerState env_sampler)
{
    float3 specular = 0;
    const uint num_samples = 40;

    float a = roughness*roughness;

    for(uint i = 0; i < num_samples; ++i)
    {
        float2 xi = hammersley2d(i, num_samples);

        float3 H = importance_sample_ggx(xi, a, N);

        float3 L = reflect(-V, H);

        float NoV = saturate( dot( N, V ) );
        float NoL = saturate( dot( N, L ) );
        float NoH = saturate( dot( N, H ) );
        float VoH = saturate( dot( V, H ) );

        if(NoL > 0)
        {
            float2 p = latlong(L);

            float G = G_UE4(a, NoV);
            float Fc = pow(1 - VoH, 5);
            float3 F = (1 - Fc) * specular_albedo + Fc;

            // Incident light = sample * NoL
            // Microfacet specular = D*G*F / (4*NoL*NoV)
            // pdf = D * NoH / (4 * VoH)
            // specular = integral (Incident light * microfacet specular)/pdf

            uint w, h;
            env_map.GetDimensions(w, h);

            float pdf = D_ggx(a, NoH) * NoH / (4 * VoH);
            float lod = compute_lod(H, pdf, num_samples, w, h);

            float3 sample = pow(abs(env_map.SampleLevel(env_sampler, p.xy, lod).rgb), GAMMA);

            specular += sample * (F * G * VoH / (NoH * NoV));
        }
    }

    return specular / num_samples;
}

// Importance sample a specular Phong function from an environment map.
// Morgan McGuire et al., Plausible Blinn-Phong Reflection of Standard Cube MIP-Maps
// http://graphics.cs.williams.edu/papers/EnvMipReport2013/
float3 specular_ibl_is_bphong(in float3 diffuse, in float3 specular, in float roughness, in float3 N, in float3 V, in Texture2D env_map, in SamplerState env_sampler)
{
    uint w, h;
    env_map.GetDimensions(w, h);

    float glossyExponent = alpha_to_spec_pow(roughness*roughness);

    float MIPlevel = log2(w * sqrt(3)) - 0.5 * log2(glossyExponent + 1);

    float3 R = reflect(-V, N);
    float2 pReflect = latlong(R);

    return specular * pow(env_map.SampleLevel(env_sampler, pReflect, MIPlevel).rgb, GAMMA);
}

#endif
