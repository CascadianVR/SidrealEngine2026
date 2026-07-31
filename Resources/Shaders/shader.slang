const static float3 positions[3] = {
    float3(-1.0, 1.0, 0.0),
    float3( 1.0, 1.0, 0.0),
    float3( 0.0,  -1.0, 0.0)
};

const static float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 Color;
};

[shader("vertex")]
VSOutput main(uint vertexIndex : SV_VertexID)
{
    VSOutput output;

    output.Position = float4(
        positions[vertexIndex],
        1.0
    );

    output.Color = colors[vertexIndex];

    return output;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET
{
    return float4(input.Color, 1.0);
}