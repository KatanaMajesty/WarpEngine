// https://youtu.be/RRE-F57fbXw?t=560 timecode of the whole thing
// In Pbr we want energy conservation, thus assertion -> Kd + Ks = 1

// Fresnel effect dictates the amount of specular reflection that takes place for
// the provided angle
// 
// Basically we can represent the Fresnel's factor as Ks specular coefficient
// in float F0 - material's base reflectivity (reflectivity when the viewing angle is perpendicular to the surface, aka colinear with surface's normal)
// in float3 V - normalized view vector
// in float3 H - normalized half-vector between view vector V and light vector L
// returns Schlick's approximation of Fresnel factor (https://en.wikipedia.org/wiki/Schlick%27s_approximation)
float3 Brdf_FresnelSchlick(in float3 F0, in float VdotH)
{
    return F0 + (1.0 - F0) * (1.0 - pow(VdotH, 5.0));
}

// TODO: Move math-related constants to separate Math.hlsli (?)
static const float PI = 3.14159265;
static const float PI_INV = 1.0 / PI;

// Lambertian Diffuse Model
// in float3 albedo - Albedo of a material
float3 Brdf_Diffuse_Lambertian(in float3 albedo)
{
    return albedo * PI_INV;
}

// Normal distribution function -> GGX/Trowbridge-Reitz model
// function determines how microfacets on the point are distributed according to roughness
// in float alpha - bascially is roughness^2
float Ndf_GGXTrowbridgeReitz(in float alpha, in float NdotH)
{
    float alphaSq = alpha * alpha;
    float distribution = (NdotH * NdotH) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * distribution * distribution);
}

// Schlick approximated the Smith equation for Beckmann. Naty warns that Schlick approximated the wrong version of Smith, 
// so be sure to compare to the Smith version before using
// in XdotN is a dot product between N and vector X (Primarily, vector X would be either view vector or light vector depending on the context).
float Gsf_SchlickBeckmann(in float k, in float XdotN)
{
    float denom = 1.0 / (XdotN * (1.0 - k) + k);
    return XdotN * denom;
}

// Geometry shadowing function -> Smith/Schlick-Beckmann model (Schlick-GGX model)
// Smith model takes into acount two types of geometry interaction between L, N and V, N vectors
float Gsf_GGXSchlick(in float alpha, in float VdotN, in float LdotN)
{
    // Schlick-GGX k coefficient remapping
    float k = alpha * 0.5;
    return Gsf_SchlickBeckmann(k, VdotN) * Gsf_SchlickBeckmann(k, LdotN);
}

// Cook-Torrance Specular Model
// in float F - fresnel factor. In most common case the result of Brdf_FresnelSchlick(F0, VdotH)
float3 Brdf_Specular_CookTorrance(in float3 F, in float roughness, in float VdotN, in float LdotN, in float VdotH, in float NdotH)
{
    float denom = 1.0 / (4.0 * VdotN * LdotN);
    
    // Normal distribution function
    float alpha = roughness * roughness;
    float Ndf = Ndf_GGXTrowbridgeReitz(alpha, NdotH);
    
    // Geometry shadowing function
    float Gsf = Gsf_GGXSchlick(alpha, VdotN, LdotN);
    return Ndf * Gsf * F * denom;
}