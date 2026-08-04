#include "Primitives.h"

void Primitives::CreateCube(Model& model)
{
    const std::vector<Vertex> vertices = {
        // Front (-Z)
        { {-0.5f, -0.5f, -0.5f, 1.0f}, { 0.0f,  0.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f, 1.0f}, { 0.0f,  0.0f, -1.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f, 1.0f}, { 0.0f,  0.0f, -1.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f, 1.0f}, { 0.0f,  0.0f, -1.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },

        // Back (+Z)
        { {-0.5f, -0.5f,  0.5f, 1.0f}, { 0.0f,  0.0f,  1.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f, 1.0f}, { 0.0f,  0.0f,  1.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f, 1.0f}, { 0.0f,  0.0f,  1.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f, 1.0f}, { 0.0f,  0.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },

        // Left (-X)
        { {-0.5f, -0.5f,  0.5f, 1.0f}, {-1.0f,  0.0f,  0.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },
        { {-0.5f, -0.5f, -0.5f, 1.0f}, {-1.0f,  0.0f,  0.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f, 1.0f}, {-1.0f,  0.0f,  0.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f, 1.0f}, {-1.0f,  0.0f,  0.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },

        // Right (+X)
        { { 0.5f, -0.5f, -0.5f, 1.0f}, { 1.0f,  0.0f,  0.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f, 1.0f}, { 1.0f,  0.0f,  0.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f, 1.0f}, { 1.0f,  0.0f,  0.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f, 1.0f}, { 1.0f,  0.0f,  0.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },

        // Bottom (-Y)
        { {-0.5f, -0.5f,  0.5f, 1.0f}, { 0.0f, -1.0f,  0.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f, 1.0f}, { 0.0f, -1.0f,  0.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f, 1.0f}, { 0.0f, -1.0f,  0.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { {-0.5f, -0.5f, -0.5f, 1.0f}, { 0.0f, -1.0f,  0.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },

        // Top (+Y)
        { {-0.5f,  0.5f, -0.5f, 1.0f}, { 0.0f,  1.0f,  0.0f, 0.0f }, {0.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f, 1.0f}, { 0.0f,  1.0f,  0.0f, 0.0f }, {1.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f, 1.0f}, { 0.0f,  1.0f,  0.0f, 0.0f }, {1.0f, 1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f, 1.0f}, { 0.0f,  1.0f,  0.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 0.0f} },
    };

    const std::vector<uint32_t> indices = {
        0, 2, 1, 2, 0, 3,       // Front
        4, 6, 5, 6, 4, 7,       // Back
        8,10, 9,10, 8,11,       // Left
       12,14,13,14,12,15,       // Right
       16,18,17,18,16,19,       // Bottom
       20,22,21,22,20,23        // Top
    };

	Mesh meshData;
    meshData.vertices = vertices;
    meshData.indices = indices;
    meshData.vertexCount = static_cast<uint32_t>(vertices.size());
    meshData.indexCount = static_cast<uint32_t>(indices.size());
    
    model.meshes.push_back(meshData);
    model.instanceCount = 1;
}

void Primitives::CreateQuad(Model& model)
{
    const std::vector<Vertex> vertices = {
        { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 0.0f} },
        { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} },
    };

    const std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    Mesh meshData;
    meshData.vertices = vertices;
    meshData.indices = indices;
    meshData.vertexCount = static_cast<uint32_t>(vertices.size());
    meshData.indexCount = static_cast<uint32_t>(indices.size());

    model.meshes.push_back(meshData);
    model.instanceCount = 1;
}
