struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
};

struct PushConstants
{
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
    float4x4 viewProjection;
    float4x4 model;
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
VSOutput main(uint32_t vertexID : SV_VertexID)
{
    VSOutput output;

    Vertex *vertices = (Vertex *)gPushConstants.vertexBuffer;
    uint32_t *indices = (uint32_t*)gPushConstants.indexBuffer;

    uint32_t index = indices[vertexID];
    Vertex v = vertices[index];

    // Object space -> world space
    float4 worldPosition = mul(gPushConstants.model, float4(v.position.xyz, 1.0));
    
    // World space -> clip space
    output.position = mul(gPushConstants.viewProjection, worldPosition);
    
    output.normal = v.normal.xyz;
    output.uv = v.uv.xy;
    output.worldPos = worldPosition;

    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET
{
    float3 lightDir = float3(0.5f, -0.5f, 0.5f);
    float3 normal = normalize(input.normal);
    return float4(normal, 1.0);
}