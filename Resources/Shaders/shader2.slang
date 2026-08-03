struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
};

// Per-Instance Data
struct InstanceData {
    float4x4 modelMatrix; // Transform of this instance
};

// Per-Mesh Render Data
struct RenderData
{
    uint32_t vertexOffset;   // Offset in the vertex buffer
    uint32_t indexOffset;    // Offset in the index buffer
    uint32_t indexCount;     // Number of indices in this mesh
    uint32_t instanceOffset; // Offset in the instance buffer
    uint32_t instanceCount;  // Number of instances to render
};

struct PushConstants {
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
    uint64_t renderDataBuffer;
    uint64_t instanceDataBuffer;
    float4x4 viewProjection;
};

[[vk::push_constant]]
PushConstants gPushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<Vertex> g_Vertices;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> g_Indices;

[[vk::binding(2, 0)]]
StructuredBuffer<InstanceData> g_Instances;

[[vk::binding(3, 0)]]
StructuredBuffer<RenderData> g_RenderDatas;

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 worldPos : TEXCOORD1;
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
    output.uv = v.uv.xy;
    output.worldPos = worldPosition;

    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 lightDir = float3(0.5f, 0.5f, 0.5f);

    float NdotL = max(dot(normal, normalize(lightDir)) * 0.5f + 0.5f, 0.0);

    return float4((normal * 0.5 + 0.5) * NdotL.xxx, 1.0);
}