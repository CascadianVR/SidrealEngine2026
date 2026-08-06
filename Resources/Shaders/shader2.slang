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

static const float EPSILON = 0.01;

bool InShadow(float3 P, float3 lightDir)
{
    // Build the shadow ray from the world position toward the light
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = P;
    shadowRayDesc.Direction = lightDir;
    shadowRayDesc.TMin = EPSILON;
    shadowRayDesc.TMax = 1e4;

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

    // Darken if in shadow
    if (InShadow(input.worldPosition, lightDir)) {
        color.rgb *= 0.1;
    }

    return float4(color.xyz, 1.0);
}