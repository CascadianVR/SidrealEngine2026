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

    // Get buffers from push constants
    Vertex* vertices = (Vertex *)gPushConstants.vertexBuffer;
    uint32_t* indices = (uint32_t*)gPushConstants.indexBuffer;
    RenderData* renderDataBuffer = (RenderData*)gPushConstants.renderDataBuffer;
    InstanceData* instanceBuffer = (InstanceData*)gPushConstants.instanceDataBuffer;

    RenderData renderData = renderDataBuffer[firstInstance];
    InstanceData instance = instanceBuffer[renderData.instanceOffset + instanceID];

    // Get vertex by calculating the index from the index buffer
    uint32_t index = indices[renderData.indexOffset + vertexID];
    Vertex v = vertices[renderData.vertexOffset + index];

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
    float3 lightDir = float3(0.5f, -0.5f, 0.5f);
    float3 normal = normalize(input.normal);
    return float4(normal * 0.5 + 0.5, 1.0);
}