struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
};

// Per-Instance Data
struct InstanceData {
    float4x4 modelMatrix; // Transform of this instance
    float2   uvTiling;
    float2   padding;
};

// Per-Mesh Render Data
struct RenderData
{
    uint32_t vertexOffset;   // Offset in the vertex buffer
    uint32_t indexOffset;    // Offset in the index buffer
    uint32_t indexCount;     // Number of indices in this mesh
    uint32_t instanceOffset; // Offset in the instance buffer
    uint32_t instanceCount;  // Number of instances to render
    uint32_t textureIndex;   // Index of the texture to use
};

struct PushConstants {
    float4x4 viewProjection;
    float4   lightDirection;
    uint32_t frameIndex;
    uint32_t padding1;
    uint32_t padding2;
    uint32_t padding3;
};

[[vk::push_constant]]
PushConstants gPushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<Vertex> g_Vertices;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> g_Indices;

[[vk::binding(2, 0)]]
StructuredBuffer<RenderData> g_RenderDatas;

[[vk::binding(3, 0)]]
StructuredBuffer<InstanceData> g_Instances;

[[vk::binding(4, 0)]]
RaytracingAccelerationStructure g_AccelerationStructure;

[[vk::binding(5, 0)]]
SamplerState g_Sampler;

[[vk::binding(6, 0)]]
Texture2D<float4> g_Textures[128];

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    uint32_t textureIndex : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
};

[shader("vertex")]
VSOutput main(uint32_t vertexID: SV_VertexID, uint32_t instanceID: SV_InstanceID, uint32_t firstInstance: SV_StartInstanceLocation)
{
    VSOutput output;

    // Get render and instance data from buffers
    RenderData renderData = g_RenderDatas[firstInstance];
    InstanceData instance = g_Instances[renderData.instanceOffset + instanceID];

    // Get the vertex from the vertex buffer using the index buffer
    uint32_t index = g_Indices[renderData.indexOffset + vertexID];
    Vertex v = g_Vertices[renderData.vertexOffset + index];

    // Object space -> world space
    float4 worldPosition = mul(instance.modelMatrix, float4(v.position.xyz, 1.0));
    
    // World space -> clip space
    output.position = mul(gPushConstants.viewProjection, worldPosition);

    output.normal = normalize(mul((float3x3)instance.modelMatrix, v.normal.xyz));
    output.uv = v.uv.xy * instance.uvTiling;
    output.textureIndex = renderData.textureIndex;
    output.worldPosition = worldPosition.xyz;
    return output;
}

static const float MinRayDistance = 0.01f;
static const float MaxRayDistance = 1e4f;

float2 Hash2D(uint32_t seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    float a = float(seed & 0xFFFF) / 65535.0;
    float b = float((seed >> 16) & 0xFFFF) / 65535.0;
    return float2(a, b);
}

// Build an orthonormal basis around a direction
void BuildBasis(float3 n, out float3 t, out float3 b)
{
    float3 up = abs(n.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    t = normalize(cross(up, n));
    b = cross(n, t);
}

static const uint32_t ShadowSampleCount = 32;
float SoftShadow(float3 P, float3 geometricNormal, float3 lightDir, float lightRadius, uint2 pixelCoord, uint32_t frameIndex)
{
    float3 tangent, bitangent;
    BuildBasis(lightDir, tangent, bitangent);

    float3 origin = P + geometricNormal * 0.01;
    float occlusion = 0.0;

    for (uint32_t i = 0; i < ShadowSampleCount; i++)
    {
        // Jitter seed: unique per pixel, per sample, per frame (for temporal accumulation)
        uint32_t seed = pixelCoord.x * 1973u + pixelCoord.y * 9277u + i * 26699u + frameIndex * 3701u;
        float2 xi = Hash2D(seed);

        // Uniform sample on a disk, scaled by light radius
        float r = sqrt(xi.x) * lightRadius;
        float theta = xi.y * 6.2831853;
        float3 jitteredDir = normalize(lightDir + tangent * (r * cos(theta)) + bitangent * (r * sin(theta)));

        RayDesc shadowRayDesc;
        shadowRayDesc.Origin = origin;
        shadowRayDesc.Direction = jitteredDir;
        shadowRayDesc.TMin = MinRayDistance;
        shadowRayDesc.TMax = MaxRayDistance;

        RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> sq;
        let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        sq.TraceRayInline(g_AccelerationStructure, rayFlags, 0xFF, shadowRayDesc);
        while (sq.Proceed()) {}

        if (sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
            occlusion += 1.0;
    }

    return occlusion / float(ShadowSampleCount); // 0 = fully lit, 1 = fully shadowed
}

bool HardShadow(float3 P, float3 lightDir)
{
    // Build the shadow ray from the world position toward the light
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = P;
    shadowRayDesc.Direction = lightDir;
    shadowRayDesc.TMin = MinRayDistance;
    shadowRayDesc.TMax = MaxRayDistance;

    // Initialize a ray query for shadows
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> sq;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    sq.TraceRayInline(g_AccelerationStructure, rayFlags, 0xFF, shadowRayDesc);

    while (sq.Proceed())
    {
    }

    // If the shadow ray intersects an opaque triangle, we consider the pixel in shadow
    bool hit = (sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT);

    return hit;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(gPushConstants.lightDirection.xyz);

    float NdotL = max(dot(normal, lightDir) * 0.5f + 0.5f, 0.5);
    NdotL = smoothstep(0.3, 0.6, NdotL);

    float4 color = g_Textures[input.textureIndex].Sample(g_Sampler, input.uv);

    //// Darken if in shadow
    //if (HardShadow(input.worldPosition, lightDir)) {
    //    color.rgb *= 0.1;
    //}

    float shadow = 1 - SoftShadow(input.worldPosition, normal, lightDir, 0.01f, uint2(input.position.xy), gPushConstants.frameIndex);
    shadow = clamp(shadow, 0.1, 1.0);

    return float4(color.xyz * shadow, 1.0);
}