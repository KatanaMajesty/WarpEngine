// From "Survey of Efficient Representations for Independent Unit Vectors" by Williams College, Ubisoft and others
// https://jcgt.org/published/0003/02/01/ - publishing link

// Returns 1.0 for component if it is more than zero, otherwise -1.0
float2 Oct16_IsSignV(in float2 V)
{
    return float2((V.x > 0.0) ? 1.0 : -1.0, (V.y > 0.0) ? 1.0 : -1.0);
}

// Fast packing of 3-component vector. For precise implmenetation see Oct16_PrecisePack
// in float3 V - normalized vector to pack
// returns float2 vector in range [-1, 1]
float2 Oct16_FastPack(in float3 V)
{
    // Project the sphere onto the octahedron, and then onto the xy plane
    float2 P = V.xy * (1.0 / (abs(V.x) + abs(V.y) + abs(V.z)));
    
    // Reflect the folds of the lower hemisphere over the diagonals
    return (V.z <= 0.0) ? ((1.0 - abs(P.yx)) * Oct16_IsSignV(P)) : P;
}

// Fast unpacking of 2-component vector. If incoming 2-component vector is an ouput from Oct16_FastPack the resulting vector 
// should be relatively close to the vector provided as an input to that Oct16_FastPack call
float3 Oct16_FastUnpack(in float2 E)
{
    float3 V = float3(E.xy, 1.0 - abs(E.x) - abs(E.y));
    if (V.z < 0.0)
        V.xy = (1.0 - abs(V.yx)) * Oct16_IsSignV(V.xy);
    
    return normalize(V);
}
