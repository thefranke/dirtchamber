/* 
 * vct_tools.hlsl by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "common.h"

Texture3D<float4> v_normal  : register(t7);
Texture3D<float4> v_rho		: register(t8);

#ifdef DVCT
Texture3D<float4> v_delta   : register(t9);
#endif

SamplerState SVOFilter      : register(s1);

cbuffer svo_parameters      : register(b7)
{
    float4x4 world_to_svo   : packoffset(c0);
    float4 bb_min           : packoffset(c4);
    float4 bb_max           : packoffset(c5);
};

// Check if a point P is inside the world space SVO volume.
bool in_svo(in float3 P)
{
    float3 mi = mul(world_to_svo, bb_min).xyz;
    float3 ma = mul(world_to_svo, bb_max).xyz;

    return (P.x > mi.x && P.y > mi.y && P.z > mi.z) &&
           (P.x < ma.x && P.y < ma.y && P.z < ma.z);
}
 
// Check if a Ray (P, V) intersect with the SVO volume to early out ray tracing.
bool intersect(in float3 P, in float3 V, in float t0, in float t1)
{
    // rotate bbox with world matrix of object before transforming!
    float3 mi = mul(world_to_svo, bb_min).xyz;
    float3 ma = mul(world_to_svo, bb_max).xyz;

    float3 bounds[2] = { mi, ma };

    float tmin, tmax, tymin, tymax, tzmin, tzmax;

    if (V.x >= 0)
    {
        tmin = (bounds[0].x - P.x) / V.x;
        tmax = (bounds[1].x - P.x) / V.x;
    }
    else
    {
        tmin = (bounds[1].x - P.x) / V.x;
        tmax = (bounds[0].x - P.x) / V.x;
    }

    if (V.y >= 0)
    {
        tymin = (bounds[0].y - P.y) / V.y;
        tymax = (bounds[1].y - P.y) / V.y;
    }
    else
    {
        tymin = (bounds[1].y - P.y) / V.y;
        tymax = (bounds[0].y - P.y) / V.y;
    }

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    if (V.z >= 0)
    {
        tzmin = (bounds[0].z - P.z) / V.z;
        tzmax = (bounds[1].z - P.z) / V.z;
    }
    else
    {
        tzmin = (bounds[1].z - P.z) / V.z;
        tzmax = (bounds[0].z - P.z) / V.z;
    }

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    if (tzmax < tmax)
        tmax = tzmax;

    return ((tmin < t1) && (tmax > t0));
}

#ifdef DVCT
// Fetch a voxel from the SVO (V_mu or V_rho) with colors and average occlusion.
float4 voxel_fetch(in float3 sample_pos, in float3 vV, in float mip, in bool is_real_surface)
{
    float3 v_d = v_delta.SampleLevel(SVOFilter, sample_pos, mip).rgb;
    float3 v_r = v_rho.SampleLevel(SVOFilter, sample_pos, mip).rgb;
    float4 N = v_normal.SampleLevel(SVOFilter, sample_pos, mip).rgba;
    float occlusion = N.a;

    if (is_real_surface)
        return float4(v_d, occlusion);
    else
        return float4(v_r, occlusion);
}
#endif

// Fetch a voxel from the SVO with colors and average occlusion.
float4 voxel_fetch(in float3 sample_pos, in float3 vV, in float mip)
{
#ifndef UNFILTERED
    float3 bounce = v_rho.SampleLevel(SVOFilter, sample_pos, mip).rgb;
    float4 Nrgba = v_normal.SampleLevel(SVOFilter, sample_pos, mip).rgba;
#else
    float3 bounce = v_rho[sample_pos*SVO_SIZE];
    float4 Nrgba = v_normal[sample_pos*SVO_SIZE];
#endif

    float3 N = Nrgba.rgb * 2.0 - 1.0;
    float occlusion = Nrgba.a;

    return float4(bounce, occlusion);
}

// Origin, dir, and max_dist are in texture space
// dir should be normalized
// cone_ratio is the cone diameter to height ratio (2.0 for 90-degree cone)
#ifdef DVCT
float4 trace_cone(in float3 origin, in float3 dir, in float cone_ratio, in float max_dist, in float bias, in bool is_real_surface)
#else
float4 trace_cone(in float3 origin, in float3 dir, in float cone_ratio, in float max_dist, in float bias)
#endif
{
    if (!intersect(origin, dir, 0, 1)) 
        return 0;

    // minimum diameter is half the sample size to avoid hitting empty space
    float min_voxel_diameter = 0.5 / SVO_SIZE;
    float min_voxel_diameter_inv = 1.0 / min_voxel_diameter;

    float4 accum = 0.f;

    // max step counter
    uint steps = 0;

    // push out the starting point to avoid self-intersection
    float dist = min_voxel_diameter;
    
    dist *= bias * cone_ratio;

    bool entered_svo = false;

#ifdef DVCT
    while (dist < max_dist && accum.w < 1.0f /*&& steps < 800*/)
#else
    while (dist < max_dist && accum.w < 0.05 / dist)
#endif
    {
        float sample_diameter = max(min_voxel_diameter, cone_ratio * dist);

        float sample_lod = log2(sample_diameter * min_voxel_diameter_inv);

        float3 sample_pos = origin + dir * dist;

        if (!in_svo(sample_pos))
        {
            if (entered_svo)
                break;
        }
        else
            entered_svo = true;

        dist += sample_diameter;

#ifdef DVCT
        float4 sample_value = voxel_fetch(sample_pos, -dir, sample_lod, is_real_surface);
#else
        float4 sample_value = voxel_fetch(sample_pos, -dir, sample_lod);
#endif

        float a = 1.0 - accum.w;
        accum += sample_value * a;

        steps++;
    }

    return accum;
}

// Trace a shadow cone in the SVO volume (P, V, cone_ratio) with a maximum distance of max_dist and a step multiplier step_mult.
// Returns the average occlusion.
float trace_shadow_cone(in float3 P, in float3 V, in float cone_ratio, in float max_dist, in float step_mult)
{
    if (!intersect(P, V, 0, 1))
        return 1;

    float min_voxel_diameter = 0.5 / SVO_SIZE;
    float min_voxel_diameter_inv = 1.0 / min_voxel_diameter;

    float accum = 0.f;

    // push out the starting point to avoid self-intersection
    float dist = min_voxel_diameter;

    float entered_svo = false;

    while (dist <= max_dist && accum < 1.0f)
    {
        float sample_diameter = max(min_voxel_diameter, cone_ratio * dist);

        float sample_lod = log2(sample_diameter * min_voxel_diameter_inv);

        float3 sample_pos = P + V * dist;

        if (!in_svo(sample_pos))
        {
            if (entered_svo)
                break;
        }
        else
            entered_svo = true;

        dist += sample_diameter * step_mult;

        float sample_value = v_normal.SampleLevel(SVOFilter, sample_pos, sample_lod).a;

        float a = 1.0 - accum;
        accum += sample_value * a;
    }

    float shadowing = saturate(1.0 - accum);

    return shadowing;
}

// Trace a shadow cone in world space.
float shadow_cone(in float3 P, in float3 L, in float3 N, in float cone_angle)
{
    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;
    float3 vL = mul(world_to_svo, float4(L, 0.0)).xyz;
    float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

    // bias a bit to avoid self intersection
    vP += normalize(vN) / SVO_SIZE * 5;
    vP += dot(normalize(vN), normalize(vL)) * normalize(vL) / SVO_SIZE;

    return saturate(trace_shadow_cone(vP, vL, cone_angle, 500, 50));
}

// Compute AO with shadow cones in world space.
float shadow_ao(in float3 P, in float3 L, in float3 N, in float cone_angle)
{
    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;
    float3 vL = mul(world_to_svo, float4(L, 0.0)).xyz;
    float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

    // bias a bit to avoid self intersection
    vP += normalize(vN) / SVO_SIZE;
    
    return saturate(trace_shadow_cone(vP, vL, cone_angle, 2, 8))+0.2;
}

#ifdef DVCT
float4 specular_from_vct(in float3 P, in float3 N, in float3 V, float cone_angle, bool is_real)
#else
float4 specular_from_vct(in float3 P, in float3 N, in float3 V, float cone_angle)
#endif
{
    float3 R = reflect(-V, N);

    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;
    float3 vR = mul(world_to_svo, float4(R, 0.0)).xyz;
    float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

    // bias a bit to avoid self intersection
    vP += 4 * normalize(vN) / SVO_SIZE;

    float NdotL = saturate(dot(normalize(N), normalize(R)));

    // get more fingerained specular refs here
    float3 vvR = normalize(vR) / 0.5;

#ifdef DVCT
    float4 spec_bounce = trace_cone(vP, vvR, cone_angle, 11, 1, is_real);
#else
    float4 spec_bounce = trace_cone(vP, vvR, cone_angle, 11, 1);
#endif

    // TODO: add actual material properties here
    float3 f = brdf(R, V, N, 1, 1, 0);

    return float4(spec_bounce.rgb * NdotL * f.rgb, spec_bounce.w);
}

#ifdef DVCT
float4 diffuse_from_vct(in float2 tc, in float3 P, in float3 N, in float3 V, bool is_real)
#else
float4 diffuse_from_vct(in float2 tc, in float3 P, in float3 N, in float3 V)
#endif
{
    float3 vP = mul(world_to_svo, float4(P, 1.0)).xyz;

    // TODO: generate better cones! This hack will likely fail at some point
    float3 diffdir = normalize(N.zxy);
    float3 crossdir = cross(N.xyz, diffdir);
    float3 crossdir2 = cross(N.xyz, crossdir);

    // jitter cones
    float j = 1.0 + (frac(sin(dot(tc, float2(12.9898, 78.233))) * 43758.5453)) * 0.2;

    float3 directions[9] =
    {
        N,
        normalize(crossdir   * j + N),
        normalize(-crossdir  * j + N),
        normalize(crossdir2  * j + N),
        normalize(-crossdir2 * j + N),
        normalize((crossdir + crossdir2)  * j + N),
        normalize((crossdir - crossdir2)  * j + N),
        normalize((-crossdir + crossdir2) * j + N),
        normalize((-crossdir - crossdir2) * j + N),
    };

    float diff_angle = 0.6f;

    float4 diffuse = float4(0, 0, 0, 0);

#define num_d 9

    for (uint d = 0; d < num_d; ++d)
    {
        float3 D = directions[d];
        float3 vD = mul(world_to_svo, float4(D, 0.0)).xyz;
        float3 vN = mul(world_to_svo, float4(N, 0.0)).xyz;

        vP += normalize(vN) / SVO_SIZE;

        float NdotL = saturate(dot(normalize(N), normalize(D)));

        // TODO: add actual material properties here
        float4 f = float4(brdf(normalize(D), V, N, 1, 0, 1), 1);

#ifdef DVCT
        diffuse += trace_cone(vP, normalize(vD), diff_angle, 5, 20, is_real) * NdotL * f;
#else
        diffuse += trace_cone(vP, normalize(vD), diff_angle, 2, 5) * NdotL * f;
#endif
    }

    diffuse *= 4.0 * M_PI / num_d;

    return diffuse / M_PI;
}
