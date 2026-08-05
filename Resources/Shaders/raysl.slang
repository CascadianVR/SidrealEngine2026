struct Vertex
{
    float4 position;
    float4 normal;
    float4 uv;
};


struct InstanceData
{
    float4x4 modelMatrix;
};


struct RenderData
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t instanceOffset;
    uint32_t instanceCount;
    uint32_t textureIndex;
};


struct RayPushConstants
{
    float4x4 view;
    float4x4 projection;
    float4 lightDirection;
    uint2 resolution;
};


[[vk::push_constant]]
RayPushConstants g_PushConstants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> g_OutputImage;

[[vk::binding(1, 0)]]
RaytracingAccelerationStructure g_TLAS;

[[vk::binding(2, 0)]]
StructuredBuffer<Vertex> g_Vertices;

[[vk::binding(3, 0)]]
StructuredBuffer<uint32_t> g_Indices;

[[vk::binding(4, 0)]]
StructuredBuffer<RenderData> g_RenderDatas;

[[vk::binding(5, 0)]]
StructuredBuffer<InstanceData> g_Instances;

// ------------------------------------------------------------
// Matrix inverse
// ------------------------------------------------------------

float4x4 inverseMatrix(float4x4 m)
{
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
    float a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];

    float b00 = a00 * a11 - a01 * a10;
    float b01 = a00 * a12 - a02 * a10;
    float b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11;
    float b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30;
    float b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31;
    float b10 = a21 * a33 - a23 * a31;
    float b11 = a22 * a33 - a23 * a32;

    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (abs(det) < 1e-6) return m;
    det = 1.0 / det;

    return float4x4(
        (a11 * b11 - a12 * b10 + a13 * b09) * det,
        (a02 * b10 - a01 * b11 - a03 * b09) * det,
        (a31 * b05 - a32 * b04 + a33 * b03) * det,
        (a02 * b04 - a01 * b05 - a03 * b03) * det,
        (a12 * b08 - a10 * b11 - a13 * b07) * det,
        (a00 * b11 - a02 * b08 + a03 * b07) * det,
        (a32 * b02 - a30 * b05 - a33 * b01) * det,
        (a01 * b08 - a00 * b07 + a03 * b01) * det,
        (a10 * b10 - a11 * b08 + a13 * b06) * det,
        (a01 * b07 - a00 * b10 - a03 * b06) * det,
        (a30 * b04 - a31 * b02 + a33 * b00) * det,
        (a00 * b06 - a01 * b07 + a03 * b02) * det,
        (a11 * b07 - a10 * b09 - a12 * b06) * det,
        (a00 * b09 - a01 * b08 + a02 * b06) * det,
        (a31 * b01 - a30 * b03 - a32 * b00) * det,
        (a01 * b03 - a00 * b04 + a02 * b01) * det
    );
}


// ------------------------------------------------------------
// Camera ray
// ------------------------------------------------------------

RayDesc CreatePrimaryRay(uint2 pixelCoord)
{
    float2 uv =
        (float2(pixelCoord) + 0.5f) /
        float2(g_PushConstants.resolution);


    float2 ndc =
        uv * 2.0f - 1.0f;


    ndc.y = -ndc.y;


    float4 clip =
        float4(ndc.x, ndc.y, -1.0f, 1.0f);


    float4 viewPos =
        mul(
            inverseMatrix(g_PushConstants.projection),
            clip
        );


    viewPos /= viewPos.w;
    viewPos.z = -1.0f;


    float4x4 invView =
        inverseMatrix(g_PushConstants.view);


    RayDesc ray;


    ray.Origin =
        mul(
            invView,
            float4(0,0,0,1)
        ).xyz;


    float3 target =
        mul(
            invView,
            viewPos
        ).xyz;


    ray.Direction =
        normalize(target - ray.Origin);


    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;


    return ray;
}


// ------------------------------------------------------------
// Fetch interpolated normal
// ------------------------------------------------------------

float3 GetHitNormal(
    RayQuery<RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq)
{
    uint renderIndex =
        rq.CommittedInstanceID();


    uint primitiveID =
        rq.CommittedPrimitiveIndex();


    RenderData renderData =
        g_RenderDatas[renderIndex];


    uint index0 =
        g_Indices[
            renderData.indexOffset +
            primitiveID * 3 + 0
        ];

    uint index1 =
        g_Indices[
            renderData.indexOffset +
            primitiveID * 3 + 1
        ];

    uint index2 =
        g_Indices[
            renderData.indexOffset +
            primitiveID * 3 + 2
        ];


    Vertex v0 =
        g_Vertices[
            renderData.vertexOffset + index0
        ];

    Vertex v1 =
        g_Vertices[
            renderData.vertexOffset + index1
        ];

    Vertex v2 =
        g_Vertices[
            renderData.vertexOffset + index2
        ];


    float2 bary =
        rq.CommittedTriangleBarycentrics();


    float3 objectNormal =
        v0.normal.xyz * (1.0f - bary.x - bary.y) +
        v1.normal.xyz * bary.x +
        v2.normal.xyz * bary.y;


    objectNormal =
        normalize(objectNormal);


    // Equivalent to:
    // normalize(mul((float3x3)instance.modelMatrix, normal))
    //
    // but works correctly with non-uniform scale.

    float3x3 normalMatrix =
        transpose(
            (float3x3)
            rq.CommittedWorldToObject3x4()
        );


    return normalize(
        mul(normalMatrix, objectNormal)
    );
}


// ------------------------------------------------------------
// Ray generation
// ------------------------------------------------------------

[shader("raygeneration")]
void RayGenShader([[vk::builtin("gl_LaunchIdKHR")]] in uint3 dispatchRaysIndex)
{
    uint2 pixel = dispatchRaysIndex.xy;


    if(pixel.x >= g_PushConstants.resolution.x ||
       pixel.y >= g_PushConstants.resolution.y)
    {
        return;
    }


    RayDesc ray =
        CreatePrimaryRay(pixel);


    float3 lightDir =
        normalize(
            g_PushConstants.lightDirection.xyz
        );


    float skyT =
        saturate(
            ray.Direction.y * 0.5f + 0.5f
        );


    float3 pixelColor =
        lerp(
            float3(0.1f,0.2f,0.4f),
            float3(0.5f,0.7f,1.0f),
            skyT
        );


    RayQuery<RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;


    rq.TraceRayInline(
        g_TLAS,
        RAY_FLAG_FORCE_OPAQUE,
        0xFF,
        ray
    );


    while(rq.Proceed())
    {
    }


    if(rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        float hitT =
            rq.CommittedRayT();


        float3 hitPos =
            ray.Origin +
            ray.Direction *
            hitT;


        float3 N =
            GetHitNormal(rq);



        float3 albedo =
            0.5f +
            0.5f *
            sin(
                hitPos.xzx * 0.5f +
                float3(0,1,2)
            );



        RayDesc shadowRay;


        shadowRay.Origin =
            hitPos + N * 0.002f;


        shadowRay.Direction =
            lightDir;


        shadowRay.TMin = 0.001f;
        shadowRay.TMax = 1000.0f;



        RayQuery<RAY_FLAG_FORCE_OPAQUE |
                 RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH>
            shadowRQ;


        shadowRQ.TraceRayInline(
            g_TLAS,
            RAY_FLAG_FORCE_OPAQUE |
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF,
            shadowRay
        );


        while(shadowRQ.Proceed())
        {
        }


        bool shadow =
            shadowRQ.CommittedStatus()
            == COMMITTED_TRIANGLE_HIT;



        float NdotL =
            max(
                dot(N, lightDir),
                0.0f
            );


        float3 ambient =
            albedo * 0.15f;


        float3 diffuse =
            albedo * NdotL;


        pixelColor =
            shadow ?
            ambient :
            ambient + diffuse;
    }


    g_OutputImage[pixel] =
        float4(pixelColor,1.0f);
}